#include "stm8s.h"
#include <stdio.h>
#define DQ_PIN GPIOD,GPIO_PIN_2       // DATA pin
#define VC_PIN GPIOD,GPIO_PIN_3       // VC transistor base pin

#define D_490_uS 1568 
#define D_40_uS 128
#define D_10_uS 32

//  |             /[100]---[+5V]         
//  |            C       |
// [VC]--[10K]--B      [5K]
//  |           E       |
//  |            \      |
// [DQ]------------------[DS18B20]
//  |

#define N 2 // amount of devices
short devices[N][8]; //devises serial numbers

void delay_us(int us){
  while(us) us--;
}

void sendBit(short bit){
  if(bit){
    GPIO_WriteLow(DQ_PIN);
    unsigned short i=30; //9us delay for '1'
    while(i) i--;
    GPIO_WriteHigh(DQ_PIN);
    i=200;
    while(i) i--;
  }else{
    GPIO_WriteLow(DQ_PIN);
    unsigned short i=250; //64us delay for '0'
    while(i) i--;
    GPIO_WriteHigh(DQ_PIN);
  }  
}
unsigned short readBit(){
  unsigned short bit=0;
  GPIO_WriteLow(DQ_PIN);
  unsigned short i=20; // 6us delay
  while(i) i--;
  GPIO_WriteHigh(DQ_PIN);
  if(GPIO_ReadInputPin(DQ_PIN)) bit=1;
  i=240; // delay to 60+us
  while(i) i--;
  return(bit);
}
unsigned short readByte(){
  unsigned short a=0;
  for(short i=0;i<8;i++){
    a=a>>1;
    if(readBit()) a+=128;
  }
  return(a);
}
void sendByte(unsigned short byte){
  for(unsigned short i=0;i<8;i++) sendBit(byte>>i&1);
}
short reset(){
  short status=0;
  GPIO_WriteLow(DQ_PIN); // pull down DQ for init/reset
  delay_us(D_490_uS);
  GPIO_WriteHigh(DQ_PIN); // release DQ
  delay_us(D_40_uS);
  status=GPIO_ReadInputPin(DQ_PIN);// check answer from DS18
  delay_us(D_490_uS);
  return(!status);
}

void setConfigAll(){
  if(reset()){
    sendByte(0xCC); // to all devices 
    sendByte(0x4E); // write to ROM config bytes
    sendByte(0x4B); // byte1: alarm trigger register (default)
    sendByte(0x46); // byte2: alarm trigger register (default)
    sendByte(0x1F); // byte3: set low resolution (quick convetr T)
  }
}
void searchSerialNums(){
  short snBit; // serial number bit, sended by device
  short iBit; // inverted serial number bit, sended by device
  short lastFork=-1;
  short rightFork=-1;
  for(short j=0;j<N;j++){
    if(reset()){
      sendByte(0xF0); // search SN
      for(short i=0;i<64;i++){
        short byteN=i/8;
        short bitN=i%8;
        if((bitN)==0) devices[j][byteN]=0;
        snBit=readBit();
        iBit=readBit();
        if(snBit==iBit){ // TODO do something whis errors (b==1 and ib ==1)
          if(i==lastFork) snBit=0;
          else{
            if(i>lastFork) snBit=1;
              else snBit=devices[j-1][byteN]>>bitN&1;
            if(snBit==1) rightFork=i;
          }
        }
        devices[j][byteN]=devices[j][byteN]|(snBit<<bitN);
        sendBit(snBit);
      }
      lastFork=rightFork;
    }
  }
}
short getTemp(short* sn){
  __disable_interrupt();
  GPIO_WriteLow(VC_PIN);
  short temp=0;
  if(reset()){
    sendByte(0x55); // to one devices
    for(unsigned short i=0;i<8;i++) sendByte(sn[i]); // serial number of device
    sendByte(0xBE); // read memory
    GPIO_WriteHigh(VC_PIN);
    delay_us(D_10_uS);
    GPIO_WriteLow(VC_PIN);
    unsigned short lsb=readByte(); // only first two bytes
    unsigned short msb=readByte(); 
    reset();
    temp=(msb<<4)+(lsb>>4);
  }
  __enable_interrupt();
  return(temp);
}
void convertTempAll(){
  __disable_interrupt();
  if(reset()){
    sendByte(0xCC); // to all devices
    sendByte(0x44); // conert T
    GPIO_WriteHigh(VC_PIN);
  }
  __enable_interrupt();
}
void initTempDevices(){
  __disable_interrupt();
  GPIO_Init(VC_PIN,GPIO_MODE_OUT_PP_LOW_SLOW);
  GPIO_Init(DQ_PIN,GPIO_MODE_OUT_OD_HIZ_SLOW);
  searchSerialNums();
  setConfigAll();
  __enable_interrupt();
}
void main() {
  
  CLK_SYSCLKConfig(CLK_PRESCALER_HSIDIV1);
  initTempDevices();
  short temp;
  while(1){
    convertTempAll();
    for(long i=0;i<52725;i++); // delay 95mS until temp converted (select by resolution)
    for(unsigned short i=0;i<N;i++){
      temp=getTemp(devices[i]);
      putchar(temp/10+48);
      putchar(temp%10+48);
      putchar(' ');
   }
   putchar('\n');
}
 
}
