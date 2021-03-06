#include <stm32f0xx.h>
#include <stm32f0xx_misc.h>
#include <stm32f0xx_rcc.h>
#include <stm32f0xx_crs.h>

#include <usb_core.h>
#include <usb_regs.h>
#include <usb_control.h>
#include <usb_cdc.h>

static usb_dev_t USB = {
  { {0}, {0}, {0}, {0} },
  0, 0
};

static void CRS_Config(void)
{
  /*Enable CRS Clock*/
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_CRS, ENABLE);
  
  /* Select USB SOF as synchronization source */
  CRS_SynchronizationSourceConfig(CRS_SYNCSource_USB);
  
  /*Enables the automatic hardware adjustment of TRIM bits: AUTOTRIMEN:*/
  CRS_AutomaticCalibrationCmd(ENABLE);
  
  /*Enables the oscillator clock for frequency error counter CEN*/
  CRS_FrequencyErrorCounterCmd(ENABLE);
}

void USB_Start(void) {
  /* Enable USB clock */
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_USB, ENABLE);

  /* Select HSI48 as USB clock */
  RCC_USBCLKConfig(RCC_USBCLK_HSI48);
  
  /* Configure the Clock Recovery System */
  CRS_Config();  
  
  /* Flip the Reset bits around to initialize the core */
  _SetCNTR(CNTR_FRES);
  _SetCNTR(0);

  /* Clear interrupts */
  _SetISTR(0);

  /* BTable describes the PMA layout, and must be in the PMA */
  _SetBTABLE(BTABLE_ADDRESS);

  /* Set interrupt mask */
  _SetCNTR(CNTR_CTRM | CNTR_DOVRM | CNTR_WKUPM | CNTR_SUSPM | CNTR_ERRM | CNTR_SOFM | CNTR_ESOFM | CNTR_RESETM);

  /* Connect the interrupts from the USB macrocell to the NVIC */
  NVIC_InitTypeDef NVIC_InitStructure;
  
  NVIC_InitStructure.NVIC_IRQChannel = USB_LP_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPriority = 1;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);

  /* Turn on the internal pull-up */
  *BCDR |= BCDR_DPPU;

  /* From here on out, everything is going to get handled through the USB Interrupt Handler */
}

/******************************************************************************
 * Configuration
 */

void EP_Config(uint8_t ep, uint16_t dir, uint16_t type, uint32_t addr) {
  /* PMA Table Entry */
  uint16_t* btable_entry = (uint16_t*)(PMA_BASE + BTABLE_ADDRESS);
  btable_entry += ep * 4; /* four half-words per btable entry */

  if (dir == EP_IN) {  /* for IN endpoints, we TX data to the host */
    btable_entry[0] = addr;  /* USB_ADDRn_TX */
    btable_entry[1] = 0; /* USB_COUNTn_TX */
  } else {
    btable_entry[2] = addr; /* USB_ADDRn_RX */
    /* 64-byte clock size = 
       BL_SIZE = 1 ->   1xxx xxxx xxxx xxxx (BL_SIZE=1 -> num_blocks is 32-byte increments, with 0 = 32byte)
       NUM_BLOCK = 1 ->  000 01
       Total ->         1000 0100 0000 0000 = 0x8400
    */
    btable_entry[3] = 0x8400;
  }

/* USB_EPnR register */
  _SetEPType(ep, type);

  _ClearDTOG_RX(ep);
  _ClearDTOG_TX(ep);
  
  if (dir == EP_OUT) {
    _ToggleDTOG_TX(ep);
  } else if (dir == EP_IN) {
    _ToggleDTOG_RX(ep);
  }
  
  _SetEPTxStatus(ep, EP_TX_DIS);
  _SetEPRxStatus(ep, EP_RX_DIS);

  _ClearEP_KIND(ep); /* not doing any double buffering, so just clear it */
}

/******************************************************************************
 * Interrupt handlers
 */

static void CorrectTransfer(void) {
  uint16_t istr;
  uint8_t epIndex;

  istr = _GetISTR();
  epIndex = (uint8_t)(istr & ISTR_EP_ID);

  if (epIndex == 0) {
    HandleEP0(&USB);
  } else {
    HandleCDC(&USB, epIndex);
  }

  _SetISTR((uint16_t)CLR_CTR);
}

static void Overrun(void) {
  _SetISTR((uint16_t)CLR_DOVR);
}

static void Error(void) {
  _SetISTR((uint16_t)CLR_ERR);
}

static void Wakeup(void) {
  uint16_t cntr;

  cntr = _GetCNTR();
  cntr &= (~CNTR_LPMODE);
  _SetCNTR(cntr);

  cntr = _GetCNTR();
  cntr &= (~CNTR_FSUSP);
  _SetCNTR(cntr);

  _SetISTR((uint16_t)CLR_WKUP);
}

static void Suspend(void) {
  uint16_t cntr;

  cntr = _GetCNTR();
  cntr |= CNTR_FSUSP;
  _SetCNTR(cntr);

  cntr = _GetCNTR();
  cntr |= CNTR_LPMODE;
  _SetCNTR(cntr);

  _SetISTR((uint16_t)CLR_SUSP);
}

static void Reset(void) {
  /* When we get a USB Reset, we reboot the world */

  /* Set up EP0 endpoints */
  EP_Config(0, EP_OUT, EP_CONTROL, EP0_RX_ADDR);
  EP_Config(0, EP_IN, EP_CONTROL, EP0_TX_ADDR);

  /* Arm EP0 out so we can receive our first setup packet */
  _SetEPRxStatus(0, EP_RX_VALID);
  /* And set EP0 in to NAK */
  _SetEPTxStatus(0, EP_TX_NAK);

  _SetEPAddress(0, 0);
  _SetDADDR(0 | DADDR_EF);

  USB.address = 0;
  USB.state = DEFAULT;

  _SetISTR((uint16_t)CLR_RESET);
}
static void StartOfFrame(void) {
  _SetISTR((uint16_t)CLR_SOF);
}
static void ExpectedStartOfFrame(void) {
  _SetISTR((uint16_t)CLR_ESOF);
}


void USB_LP_IRQHandler(void)
{
  volatile uint16_t istr;

  while((istr = _GetISTR()) & (0xFF00)) {
    if (istr & ISTR_CTR) { CorrectTransfer(); }
    if (istr & ISTR_DOVR) { Overrun(); }
    if (istr & ISTR_ERR) { Error(); }
    if (istr & ISTR_WKUP) { Wakeup(); }
    if (istr & ISTR_SUSP) { Suspend(); }
    if (istr & ISTR_RESET) { Reset(); }
    if (istr & ISTR_SOF) { StartOfFrame(); }
    if (istr & ISTR_ESOF) { ExpectedStartOfFrame(); }
  }
}

/**********************************************************************
 * USB Buffer Copying
 */

/**
  * @brief Copy a buffer from user memory area to packet memory area (PMA)
  * @param   pbUsrBuf: pointer to user memory area.
  * @param   wPMABufAddr: address into PMA.
  * @param   wNBytes: no. of bytes to be copied.
  * @retval None
  */
void UserToPMABufferCopy(const uint8_t *pbUsrBuf, uint16_t wPMABufAddr, uint16_t wNBytes)
{
  uint32_t n = (wNBytes + 1) >> 1; 
  uint32_t i;
  uint16_t temp1, temp2;
  uint16_t *pdwVal;
  pdwVal = (uint16_t *)(wPMABufAddr + PMAAddr);
  
  for (i = n; i != 0; i--)
  {
    temp1 = (uint16_t) * pbUsrBuf;
    pbUsrBuf++;
    temp2 = temp1 | (uint16_t) * pbUsrBuf << 8;
    *pdwVal++ = temp2;
    pbUsrBuf++;
  }
}

/**
  * @brief Copy a buffer from user memory area to packet memory area (PMA)
  * @param   pbUsrBuf    = pointer to user memory area.
  * @param   wPMABufAddr: address into PMA.
  * @param   wNBytes: no. of bytes to be copied.
  * @retval None
  */
void PMAToUserBufferCopy(uint8_t *pbUsrBuf, uint16_t wPMABufAddr, uint16_t wNBytes)
{
  uint32_t n = (wNBytes + 1) >> 1;
  uint32_t i;
  uint16_t *pdwVal;
  pdwVal = (uint16_t *)(wPMABufAddr + PMAAddr);
  for (i = n; i != 0; i--)
  {
    *(uint16_t*)pbUsrBuf++ = *pdwVal++;
    pbUsrBuf++;
  }
}
