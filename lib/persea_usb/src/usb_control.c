#include <stm32f0xx.h>
#include <stm32f0xx_misc.h>

#include <usb_core.h>
#include <usb_regs.h>
#include <usb_control.h>
#include <usb_cdc.h>

typedef struct {
  uint8_t bmRequestType;
  uint8_t bRequest;
  uint16_t wValue;
  uint16_t wIndex;
  uint16_t wLength;
} usb_setup_req_t;

typedef struct {
  const uint8_t* buf;
  uint16_t count;
  uint8_t send_zlp;
} outgoing_data_t;

static outgoing_data_t ep0_output;

/**********************************************************************
 * Descriptors
 */
#define USB_SIZ_DEVICE_DESC                     18
#define USBD_VID                        0x16d0
#define USBD_PID                        0x0af3

const uint8_t DeviceDescriptor[USB_SIZ_DEVICE_DESC] =
{
  0x12,                       /*bLength */
  DESC_DEVICE, /*bDescriptorType*/
  0x00,                       /*bcdUSB */
  0x02,
  0x00,                       /*bDeviceClass*/
  0x00,                       /*bDeviceSubClass*/
  0x00,                       /*bDeviceProtocol*/
  64,           /*bMaxPacketSize*/
  LOBYTE(USBD_VID),           /*idVendor*/
  HIBYTE(USBD_VID),           /*idVendor*/
  LOBYTE(USBD_PID),           /*idVendor*/
  HIBYTE(USBD_PID),           /*idVendor*/
  0x00,                       /*bcdDevice rel. 2.00*/
  0x02,
  0x01,           /*Index of manufacturer  string*/
  0x02,       /*Index of product string*/
  0x03,        /*Index of serial number string*/
  1            /*bNumConfigurations*/
} ; /* USB_DeviceDescriptor */

#define  USB_LEN_DEV_QUALIFIER_DESC                     0x0A
static const uint8_t DeviceQualifier[USB_LEN_DEV_QUALIFIER_DESC] =
{
  USB_LEN_DEV_QUALIFIER_DESC,
  DESC_DEVICE_QUALIFIER,
  0x00,
  0x02,
  0x00,
  0x00,
  0x00,
  0x40,
  0x01,
  0x00,
};

#define USB_CDC_CONFIG_DESC_SIZ (39)
const uint8_t CdcConfig[USB_CDC_CONFIG_DESC_SIZ] =
{
  /*Configuration Descriptor*/
  0x09,   /* bLength: Configuration Descriptor size */
  DESC_CONFIGURATION,      /* bDescriptorType: Configuration */
  USB_CDC_CONFIG_DESC_SIZ,                /* wTotalLength:no of returned bytes */
  0x00,
  0x01,   /* bNumInterfaces: 1 interface */
  0x01,   /* bConfigurationValue: Configuration value */
  0x00,   /* iConfiguration: Index of string descriptor describing the configuration */
  0x80,   /* bmAttributes: self powered */
  250,   /* MaxPower 0 mA */
  
  /*---------------------------------------------------------------------------*/
  
  /*Interface Descriptor */
  0x09,   /* bLength: Interface Descriptor size */
  DESC_INTERFACE,  /* bDescriptorType: Interface */
  /* Interface descriptor type */
  0x00,   /* bInterfaceNumber: Number of Interface */
  0x00,   /* bAlternateSetting: Alternate setting */
  0x03,   /* bNumEndpoints: Three endpoints on one interface */
  0xff,   /* bInterfaceClass: vendor-specific */
  0x01,   /* bInterfaceSubClass: Version 1 of the Peachy Commands */
  0x01,   /* bInterfaceProtocol: Version 1 of the Peachy Commands */
  0x00,   /* iInterface: */
      
  /*Endpoint 2 Descriptor*/
  0x07,                           /* bLength: Endpoint Descriptor size */
  DESC_ENDPOINT,   /* bDescriptorType: Endpoint */
  CDC_CMD_EP,                     /* bEndpointAddress */
  0x03,                           /* bmAttributes: Interrupt */
  LOBYTE(8),     /* wMaxPacketSize: */
  HIBYTE(8),
  0xFF,                           /* bInterval: */
  
  /*Endpoint OUT Descriptor*/
  0x07,   /* bLength: Endpoint Descriptor size */
  DESC_ENDPOINT,      /* bDescriptorType: Endpoint */
  CDC_OUT_EP,                        /* bEndpointAddress */
  0x02,                              /* bmAttributes: Bulk */
  LOBYTE(64),  /* wMaxPacketSize: */
  HIBYTE(64),
  0x00,                              /* bInterval: ignore for Bulk transfer */
  
  /*Endpoint IN Descriptor*/
  0x07,   /* bLength: Endpoint Descriptor size */
  DESC_ENDPOINT,      /* bDescriptorType: Endpoint */
  CDC_IN_EP,                         /* bEndpointAddress */
  0x02,                              /* bmAttributes: Bulk */
  LOBYTE(64),  /* wMaxPacketSize: */
  HIBYTE(64),
  0x00                               /* bInterval: ignore for Bulk transfer */
} ;

#define USB_LEN_LANG_DESC 4
const uint8_t LangConfig[USB_LEN_LANG_DESC] = {
  USB_LEN_LANG_DESC,
  DESC_STRING,
  0x09, 0x04 // english
};

/* Note: to use Python to encode a UTF-16-LE string descriptor, use:

', '.join(str(ord(i)) for i in x.encode('utf-16-le'))

*/

#define USB_LEN_MANUF_DESC 14
const uint8_t ManufConfig[USB_LEN_MANUF_DESC] = {
  USB_LEN_MANUF_DESC,
  DESC_STRING,
  0x50, 0x0, 0x65, 0x0, 0x61, 0x0, 0x63, 0x0, 0x68, 0x0, 0x79, 0x0
  // "Peachy" in UTF-16-LE
};

#define USB_LEN_PRODUCT_DESC 30
const uint8_t ProductConfig[USB_LEN_PRODUCT_DESC] = {
  USB_LEN_PRODUCT_DESC,
  DESC_STRING,
  0x50, 0x0, 0x65, 0x0, 0x61, 0x0, 0x63, 0x0, 0x68, 0x0, 0x79, 0x0, 0x20, 
  0x0, 0x50, 0x0, 0x72, 0x0, 0x69, 0x0, 0x6e, 0x0, 0x74, 0x0, 0x65, 0x0, 
  0x72, 0x0 // "Peachy Printer"
};

#define USB_LEN_SERIAL_DESC 18
// intentionally non-const, because it'll get injected by something else, eventually
uint8_t SerialConfig[USB_LEN_SERIAL_DESC] = {
  USB_LEN_SERIAL_DESC,
  DESC_STRING,
  0x31, 0x0, 0x32, 0x0, 0x33, 0x0, 0x34, 0x0, 0x35, 0x0, 0x36, 0x0, 0x37, 0x0, 0x38, 0x0 // 12345678
};

/** This will write 4 bytes into buf, representing b in hex */
static void byte_to_unicode(uint8_t b, uint8_t* buf) {
  uint8_t upper = (b & 0xf0) >> 4;
  uint8_t lower = (b & 0x0f);

  if (upper < 10) {
    buf[0] = 0x30 + upper;
  } else {
    buf[0] = 0x41 + (upper - 10);
  }

  buf[1] = 0;

  if (lower < 10) {
    buf[2] = 0x30 + lower;
  } else {
    buf[2] = 0x41 + (lower - 10);
  }

  buf[3] = 0;
}

void USB_SetSerial(uint32_t serial_number) {
  byte_to_unicode((serial_number & 0xff000000) >> 24, &SerialConfig[2]);
  byte_to_unicode((serial_number & 0x00ff0000) >> 16, &SerialConfig[6]);
  byte_to_unicode((serial_number & 0x0000ff00) >> 8, &SerialConfig[10]);
  byte_to_unicode((serial_number & 0x000000ff), &SerialConfig[14]);
}

#define USB_LEN_WCID_DESC 0x12
#define WCID_VENDOR_CODE 0x42
// see https://github.com/pbatard/libwdi/wiki/WCID-Devices
const uint8_t WCIDConfig[USB_LEN_WCID_DESC] = {
  USB_LEN_WCID_DESC,
  DESC_STRING,
  // MSFT100 in UTF-16-LE
  0x4D, 0x00, 0x53, 0x00,
  0x46, 0x00, 0x54, 0x00,
  0x31, 0x00, 0x30, 0x00,
  0x30, 0x00,

  WCID_VENDOR_CODE, // vendor code - made up, just has to match the other WCID descriptor
  0x00 // padding
};

#define USB_LEN_WINUSB_DESC 0x28
const uint8_t WinusbConfig[USB_LEN_WINUSB_DESC] = {
  0x28, 0x00, 0x00, 0x00, // descriptor length dword
  0x00, 0x01, // version 1.0
  0x04, 0x00, // compatibility ID
  0x01, // number of sections

  // reserved
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 

  0x00, // interface number
  0x01, // reserved

  // "WINUSB\0\0"
  0x57, 0x49, 0x4E, 0x55,
  0x53, 0x42, 0x00, 0x00,

  // subcompatible id
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,

  // reserved
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00
};

/**********************************************************************
 * Control Request Handers 
 */
void WriteEP0Ctrl(const uint8_t* buf, uint8_t size) {
  if (size) {
    UserToPMABufferCopy(buf, EP0_TX_ADDR, size);
  }

  _SetEPTxCount(0, size);
  _SetEPTxStatus(0, EP_TX_VALID);
}

void WriteEP0Status(void) {
  // Use DTOG = 1 for transmitting all EP0 Status packets
  _ClearDTOG_TX(0);
  _ToggleDTOG_TX(0);

  _SetEPTxCount(0, 0);
  _SetEPTxStatus(0, EP_TX_VALID);
}
volatile usb_setup_req_t last_setup;

static void HandleGetStringDescriptor(uint8_t idx, const uint8_t** to_send, uint8_t* to_send_size) {
  switch(idx) {
  case 0:
    *to_send = LangConfig;
    *to_send_size = sizeof(LangConfig);
    break;
  case 1:
    *to_send = ManufConfig;
    *to_send_size = sizeof(ManufConfig);
    break;
  case 2:
    *to_send = ProductConfig;
    *to_send_size = sizeof(ProductConfig);
    break;
  case 3:
    *to_send = SerialConfig;
    *to_send_size = sizeof(SerialConfig);
    break;
  case 0xee:
    *to_send = WCIDConfig;
    *to_send_size = sizeof(WCIDConfig);
  }
}

static void HandleGetDescriptor(usb_setup_req_t* setup, uint8_t* rx_buffer) {
  const uint8_t* to_send;
  uint8_t to_send_size = 0;
  last_setup = *setup;

  uint8_t descriptor = HIBYTE(setup->wValue);

  // First packet is always Data1 on control transfers
  _ClearDTOG_TX(0);
  _ToggleDTOG_TX(0);

  switch(descriptor) {
/* Only needed if it's a high speed device */
/*  case DESC_DEVICE_QUALIFIER:
    to_send = DeviceQualifier;
    to_send_size = sizeof(DeviceQualifier);
    break;
*/
  case DESC_DEVICE:
    to_send = DeviceDescriptor;
    to_send_size = sizeof(DeviceDescriptor);
    break;
  case DESC_CONFIGURATION:
    to_send = CdcConfig;
    to_send_size = sizeof(CdcConfig);
    break;
  case DESC_STRING:
    HandleGetStringDescriptor(LOBYTE(setup->wValue), &to_send, &to_send_size);
    break;
  default:
    break;
  }

  if (to_send_size) {
    if (to_send_size > setup->wLength) {
      to_send_size = setup->wLength;
    }
    ep0_output.buf = to_send;
    ep0_output.count = to_send_size;
    ep0_output.send_zlp = 0;
  }
}

static void HandleEPGetStatus(usb_dev_t* usb, usb_setup_req_t* setup, uint8_t* rx_buffer) {
  static const uint8_t status_ok[2] = { 0x00, 0x00 };
  static const uint8_t status_halted[2] = { 0x00, 0x01 };

  uint8_t endpoint = setup->wIndex & 0x7F;
  ep0_output.buf = status_halted;
  if ((endpoint < 4) && (!usb->ep[endpoint].halted)) {
    ep0_output.buf = status_ok;
  }
  ep0_output.count = 2;
  ep0_output.send_zlp = 0;
}

static void HandleEPSetFeature(usb_dev_t* usb, usb_setup_req_t* setup, uint8_t* rx_buffer, uint8_t should_set) {
  if (setup->wValue == FEATURE_ENDPOINT_HALT) {
    uint8_t endpoint = setup->wIndex & 0x7F; // ignore direction bit
    if (endpoint < 4) {
      usb->ep[endpoint].halted = should_set;
    }
    WriteEP0Status();
  }
}

static void HandleSetAddress(usb_dev_t* usb, usb_setup_req_t* setup, uint8_t* rx_buffer) {
  WriteEP0Status();

  usb->state = CHANGE_ADDRESS;
  usb->address = setup->wValue & 0x7F;

//  _SetDADDR((setup->wValue & 0x7F) | DADDR_EF);
}

static void HandleSetConfiguration(usb_dev_t* usb, usb_setup_req_t* setup, uint8_t* rx_buffer) {
  WriteEP0Status();
  usb->state = CONFIGURED;

/*  EP_Config(0, EP_OUT, EP_CONTROL, EP0_RX_ADDR);
  EP_Config(0, EP_IN, EP_CONTROL, EP0_TX_ADDR);
  _SetEPAddress(0, 0);
*/

  EP_Config(1, EP_IN, EP_INTERRUPT, EP1_TX_ADDR);
  _SetEPTxStatus(1, EP_TX_NAK);
  _SetEPAddress(1, 1);

  EP_Config(2, EP_OUT, EP_BULK, EP2_RX_ADDR);
  _SetEPRxStatus(2, EP_RX_VALID);
  _SetEPAddress(2, 2);

  EP_Config(3, EP_IN, EP_BULK, EP3_TX_ADDR);
  _SetEPTxStatus(3, EP_TX_NAK);
  _SetEPAddress(3, 3);

  CDC_SetConfiguration();
}

void DoNothingFunction() {
  volatile int i = 0;
  for(i = 0; i < 100; i++) ;
}

static void HandleVendorRequest(usb_dev_t* usb, usb_setup_req_t* setup, uint8_t* rx_buffer) {
  const uint8_t* to_send;
  uint8_t to_send_size = 0;

  if((setup->bmRequestType & 0xC0) == 0xC0 && setup->bRequest == WCID_VENDOR_CODE && setup->wIndex == 0x4) {
    // Asking for the Winusb descriptor
    to_send = WinusbConfig;
    to_send_size = sizeof(WinusbConfig);
  }

  if (to_send_size) {
    if (to_send_size > setup->wLength) {
      to_send_size = setup->wLength;
    }
    ep0_output.buf = to_send;
    ep0_output.count = to_send_size;
    ep0_output.send_zlp = 0;
  }

}

static void HandleStandardRequest(usb_dev_t* usb, usb_setup_req_t* setup, uint8_t* rx_buffer) {
  switch(setup->bmRequestType & 0x80) {
  case REQ_GET:
    switch(setup->bRequest) {
    case REQ_GET_DESCRIPTOR:
      HandleGetDescriptor(setup, rx_buffer);
      break;
    case REQ_GET_STATUS:
      if ((setup->bmRequestType & RECIPIENT_MASK) == RECIPIENT_ENDPOINT) {
        HandleEPGetStatus(usb, setup, rx_buffer);
      }
      break;
    }
    /* The Status packet is going to be DTOG=1 */
    _ClearDTOG_RX(0);
    _ToggleDTOG_RX(0);
  case REQ_SET:
    switch(setup->bRequest) {
    case REQ_SET_ADDRESS:
      HandleSetAddress(usb, setup, rx_buffer);
      break;
    case REQ_SET_CONFIGURATION:
      HandleSetConfiguration(usb, setup, rx_buffer);
      break;
    case REQ_SET_FEATURE:
      if ((setup->bmRequestType & RECIPIENT_MASK) == RECIPIENT_ENDPOINT) {
        HandleEPSetFeature(usb, setup, rx_buffer, 1);
      }
      break;
    case REQ_CLEAR_FEATURE:
      if ((setup->bmRequestType & RECIPIENT_MASK) == RECIPIENT_ENDPOINT) {
        HandleEPSetFeature(usb, setup, rx_buffer, 0);
      }
      break;
    default: 
      DoNothingFunction();
      break;
    }
    break;
  default:
    break;
  }
}

static void HandleClassGetRequest(usb_dev_t* usb, usb_setup_req_t* setup, uint8_t* rx_buffer) {
  ep0_output.count = 0;
  ep0_output.buf = 0;
  ep0_output.send_zlp = 1;

  /* get ready for a status packet */
  _ClearDTOG_RX(0);
  _ToggleDTOG_RX(0);
}

static void HandleClassSetRequest(usb_dev_t* usb, usb_setup_req_t* setup, uint8_t* rx_buffer) {
  WriteEP0Status();
}


static void HandleClassRequest(usb_dev_t* usb, usb_setup_req_t* setup, uint8_t* rx_buffer) {
  /* can we get away with just ignoring everything? */
  switch(setup->bmRequestType & 0x80) {
  case REQ_GET:
    HandleClassGetRequest(usb, setup, rx_buffer);
    break;
  case REQ_SET:
    HandleClassSetRequest(usb, setup, rx_buffer);
    break;
  }
}

static void HandleSetupPacket(usb_dev_t* usb) {
  uint8_t rx_buffer[64];
  usb_setup_req_t setup;

  uint8_t xfer_count = _GetEPRxCount(0);

  PMAToUserBufferCopy(rx_buffer, EP0_RX_ADDR, xfer_count);
  
  setup.bmRequestType = *(uint8_t *)  (rx_buffer);
  setup.bRequest      = *(uint8_t *)  (rx_buffer + 1);
  setup.wValue        = SWAPBYTE      (rx_buffer + 2);
  setup.wIndex        = SWAPBYTE      (rx_buffer + 4);
  setup.wLength       = SWAPBYTE      (rx_buffer + 6);

  switch(setup.bmRequestType & REQUEST_TYPE_MASK) {
  case REQUEST_TYPE_STD:
    HandleStandardRequest(usb, &setup, rx_buffer);
    break;
  case REQUEST_TYPE_CLASS:
    HandleClassRequest(usb, &setup, rx_buffer);
    break;
  case REQUEST_TYPE_VENDOR:
    HandleVendorRequest(usb, &setup, rx_buffer);
    break;
  default:
    DoNothingFunction();
    break;
  }

  /* Rearm the RX (EP0 OUT) endpoint */
  _SetEPRxStatus(0, EP_RX_VALID);
}

static void ChangeAddress(usb_dev_t* usb) {
  _SetDADDR(usb->address | DADDR_EF);
  usb->state = ADDRESS;
}

static void HandleControlPacket() {
  _SetEPRxStatus(0, EP_RX_VALID);
}

static void SendNextEP0() {
  if(ep0_output.count == 0 && ep0_output.send_zlp == 0) {
    return;
  }
  if (ep0_output.count == 0 && ep0_output.send_zlp) {
    ep0_output.send_zlp = 0;
    _SetEPTxCount(0,0);
    _SetEPTxStatus(0, EP_TX_VALID);
    return;
  }
  if(ep0_output.count < 64) {
    WriteEP0Ctrl(ep0_output.buf, ep0_output.count);
    ep0_output.count = 0;
    return;
  }
  /* From here on, we know we're doing a multi-part packet */
  if(ep0_output.count == 64) {
    ep0_output.send_zlp = 1;
  }
  WriteEP0Ctrl(ep0_output.buf, 64);
  ep0_output.buf += 64;
  ep0_output.count -= 64;
}

void HandleEP0(usb_dev_t* usb) {
  uint16_t istr = _GetISTR();

  if ((istr & ISTR_DIR) == 0) {
    /* This is an IN endpoint. Our transmission worked! */
    if (usb->state == CHANGE_ADDRESS) {
      ChangeAddress(usb);
    }
    
    if((istr & ISTR_EP_ID) == 0) {
      SendNextEP0();
    }
    _ClearEP_CTR_TX(0);
  } else {
    /* This is an OUT endpoint. We've got data waiting for us. */
    if (_GetENDPOINT(0) & EP_SETUP) {
      HandleSetupPacket(usb);
    } else {
      HandleControlPacket();
    }
    SendNextEP0();
    _ClearEP_CTR_RX(0);
  }
}
