#include "stm8s.h"
#include <stdio.h>
#define DQ_PORT GPIOD           // DATA port
#define DQ_PIN GPIO_PIN_2       // DATA pin
#define VC_PORT GPIOD           // VC transistor base port
#define VC_PIN GPIO_PIN_3       // VC transistor base pin

//  |             /[100]---[+5V]         
//  |            C       |
// [VC]--[10K]--B      [5K]
//  |           E       |
//  |            \      |
// [DQ]------------------[DS18B20]
//  |

#define N 4
unsigned short devices[N][8];
void delay_us(unsigned int us){
  us*=3.2;
  while(us) us--;
}
void delay_ms(unsigned long ms){
  ms*=555;
  while(ms) ms--;
}
void sendBit(unsigned short bit){
  if(bit){
    GPIO_WriteLow(DQ_PORT,DQ_PIN);
    unsigned short i=30; //9us delay for '1'
    while(i) i--;
    GPIO_WriteHigh(DQ_PORT,DQ_PIN);
    i=200;
    while(i) i--;
  }else{
    GPIO_WriteLow(DQ_PORT,DQ_PIN);
    unsigned short i=250; //64us delay for '0'
    while(i) i--;
    GPIO_WriteHigh(DQ_PORT,DQ_PIN);
  }  
}
unsigned short readBit(){
  unsigned short bit=0;
  GPIO_WriteLow(DQ_PORT,DQ_PIN);
  unsigned short i=20; // 6us delay
  while(i) i--;
  GPIO_WriteHigh(DQ_PORT,DQ_PIN);
  if(GPIO_ReadInputPin(DQ_PORT,DQ_PIN)) bit=1;
  i=240; // delay to 60+us
  while(i) i--;
  return(bit);
}
unsigned short readByte(){
  unsigned short a=0;
  for(unsigned short i=0;i<8;i++){
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
  GPIO_WriteLow(DQ_PORT,DQ_PIN); // pull down DQ for init/reset
  delay_us(490);
  GPIO_WriteHigh(DQ_PORT,DQ_PIN); // release DQ
  delay_us(40);
  status=GPIO_ReadInputPin(DQ_PORT,DQ_PIN);// check answer from DS18
  delay_us(450);
  return(!status);
}

void setConfigAll(){
  if(reset()){
    sendByte(0xCC); // to all devices 
    sendByte(0x4E); // write to ROM 3 config bytes
    sendByte(0x4B); //  alarm trigger register (default)
    sendByte(0x46); //  alarm trigger register (default)
    sendByte(0x1F); //  set low resolution (quick convetr T)
  }
}
void searchSerialNums(){
  unsigned short snBit;
  unsigned short iBit;
  short lastFork=-1;
  short rightFork=-1;
  for(unsigned short j=0;j<N;j++){
    if(reset()){
      sendByte(0xF0); // search SN
      for(short i=0;i<64;i++){
        unsigned short byteN=i/8;
        unsigned short bitN=i%8;
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
short getTemp(unsigned short* sn){
  GPIO_WriteLow(VC_PORT,VC_PIN);
  short temp=0;
  if(reset()){
    sendByte(0x55); // to all devices
    for(unsigned short i=0;i<8;i++) sendByte(sn[i]);
    sendByte(0xBE); // read memory
      GPIO_WriteHigh(VC_PORT,VC_PIN);
      delay_us(10);
        GPIO_WriteLow(VC_PORT,VC_PIN);
    unsigned short lsb=readByte(); // only first two bytes
    unsigned short msb=readByte(); 
    reset();
    temp=(msb<<4)+(lsb>>4);
  }
  return(temp);
}
void convertTempAll(){
  if(reset()){
    sendByte(0xCC); // to all devices
    sendByte(0x44); // conert T
    GPIO_WriteHigh(VC_PORT,VC_PIN);
  }
}
void initTempDevices(){
  GPIO_Init(VC_PORT,VC_PIN,GPIO_MODE_OUT_PP_LOW_SLOW);
  GPIO_Init(DQ_PORT,DQ_PIN,GPIO_MODE_OUT_OD_HIZ_SLOW);
  
  searchSerialNums();
  setConfigAll();
}
void main() {
  
  CLK_SYSCLKConfig(CLK_PRESCALER_HSIDIV1);
 /* initTempDevices();
  short temp;
  while(1){
    convertTempAll();
    delay_ms(95); // check delay for convert T resolution
   for(unsigned short i=0;i<N;i++){
      temp=getTemp(devices[i]);
      putchar(temp/10+48);
      putchar(temp%10+48);
      putchar(' ');
   }
   putchar('\n');*/
}
 
}
