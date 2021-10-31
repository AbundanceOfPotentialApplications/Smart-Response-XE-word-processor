
#include <string.h>
#include <avr/pgmspace.h>

//to do in this version:
// status bar, with battery status, page number, size and place on page

//doDo:
// minor: 
//  insert/delete
//  edit with new-lines not raw bytes
//  bookmarks
// fast-access custom special characters

// done:
//  page up/page down, and home-end maybe?
// underscore



 // needed for uint8 and stuff definitions

//#include <avr/sleep.h>
//#include <avr/interrupt.h>
//#include <util/delay.h> 
#include "../library/SmartResponseXE.h"
#include "qrcode.h"

#define C_space 32

//#define K_power			1
//#define K_powerClick	2
//#define K_shift			4
//#define K_sym			8



void Xdrawchar(byte pos, byte l)
{
//if (c&0x80)
//Colors=revese
//c=c&7f
  unsigned char tempString[2];
  tempString[0]= l&0x7f;
  tempString[1]= 0;
  if (l&0x80)
  {
	SRXEWriteString((pos&31)*12,(pos>>5)*16,tempString, FONT_MEDIUM, 0,2);
  }
  else
  {
	SRXEWriteString((pos&31)*12,(pos>>5)*16,tempString, FONT_MEDIUM, 3,0);
  }

}

	
	

byte pageBuffer[4096];

byte pageEdited;

char pageCurrent;

int cursor;
byte prevCursor;
byte prevChar;

int topScreen;

//int topLine;
//int cursorLineStart;
//int cursorX;
//int cursorY;
//int cursorYwant;

byte screenBuff[256];
//byte screenCursor;

int powerTime=0;
char blinkTime=0;

#define oneLine 32
#define oneScreen 256
// 32 bytes reserved for future improvements.
#define onePage 4062
// 4096-32


// todo: implement load leveling, linked lists editing and stuff.
void pageSave()
{
	// todo
	// encode the page order in the last 32 bytes
	// save to flash chip
	
  byte tcounter;
  uint32_t addr;
  addr=pageCurrent;
  addr*=4096;
  if (!pageEdited) return;
  SRXEFlashEraseSector(addr,1);
  for(tcounter=0;tcounter<16;tcounter++)
	SRXEFlashWritePage(addr+256*tcounter,pageBuffer+256*tcounter);
	
  pageEdited = 0;	
}
	
void pageReload()
{
	// loads current page from flash chip/

	  uint32_t addr;
	  int t;
  addr=pageCurrent;
  addr*=4096;
      SRXEFlashRead(addr,pageBuffer,4096);
    pageEdited = 0;
  for(t=0;t<onePage;t++)
    {
      if (pageBuffer[t]!=0xff) return;  
    }    
  for(t=0;t<onePage;pageBuffer[t++]=0);
  pageBuffer[4]=' ';    
  pageBuffer[5]='*';
  pageBuffer[6]='E';
  pageBuffer[7]='M';
  pageBuffer[8]='P';
  pageBuffer[9]='T';
  pageBuffer[10]='Y';
  pageBuffer[11]='*';
}	


void screenUpdate() //with autoscroll
{
  int p,t;

// autoscroll to follow cursor...
// actually it's quite cool to do the scroll in the post update...


  Xdrawchar(prevCursor, prevChar);
  
  p=topScreen;
  prevCursor=255;
  for(t=0;t<oneScreen;t++)
  {
    if (p==cursor) prevCursor=t;

    if ((p<onePage) && (pageBuffer[p]))
      {
	if (screenBuff[t]!=pageBuffer[p])
	{
	  screenBuff[t]=pageBuffer[p];
	  Xdrawchar(t,pageBuffer[p]);
	}	
      }
      else
      { // use 0 as new-line character.
        do
	{
	  if (screenBuff[t]!=' ')
	    {
	      screenBuff[t]=' ';
	      Xdrawchar(t,' ');
	    }
	  t++;
	} while (t & 31);
	t--;
      } 
    p++;
  }

  prevChar=screenBuff[prevCursor];
  if ((cursor < topScreen) || (prevCursor< oneLine))
   {
     t=31;
     if (topScreen) topScreen--;
     while((topScreen) && (t) && (pageBuffer[topScreen-1]))
       {
         topScreen--;
         t--;       
       }
   }
  else if ((prevCursor>= oneScreen-oneLine)  // includes 'cursor not found at all' case
    && (p<onePage)) // do not scroll down if we reached the end...
   {
     t=32;
     while((t) && (pageBuffer[topScreen]))
       {
         topScreen++;
         t--;       
       }
     topScreen++;
   }
  
  if (blinkTime & 32) Xdrawchar(prevCursor, prevChar ^ 0x80);
}

//void showMenu();
//void hideMenu();
int getPageEnd();

char isSpaceAt(int pos)
{
  if  ((pageBuffer[pos]>='0') && (pageBuffer[pos]<='9')) return 0;
  if  ((pageBuffer[pos]>='A') && (pageBuffer[pos]<='Z')) return 0;
  if  ((pageBuffer[pos]>='a') && (pageBuffer[pos]<='z')) return 0;
  
  
  return 1;
  
}

void statusChar(char pos, char c)
{
   unsigned char tempString[2];
  tempString[0]= c&0x7f;
  tempString[1]= 0;
  SRXEWriteString(pos*6,128,tempString, FONT_SMALL, 3,0);
  
}

void statusBarUpdate()
{ // about once per 5 seconds
  long int v;
  float adc;
  byte high,low;
  int t,t2,w;
  static char lastPercent=0;
// setup ADC for battery
  ADCSRA = 0; // Disable ADC
  ADMUX = 0xC0; // Int ref 1.6V
  ADCSRA = 0x87; // Enable ADC
  ADCSRB = 0x00; // MUX5= 0, freerun
  ADCSRC = 0x54; // Default value
  ADCSRA = 0x97; // Enable ADC
  DIDR0 = DIDR0 | ADC0D; // Disable logic buffer on ADC0
  ADCSRA |= (1 << ADSC); // start conversion


// write Page number
  statusChar(10,'p');
  statusChar(11,'0'+(pageCurrent/10)%10);
  statusChar(12,'0'+pageCurrent%10);

// count page size, and write
  t=getPageEnd();
  statusChar(15,'0'+(t/1000)%10);
  statusChar(16,'0'+(t/100)%10);
  statusChar(17,'0'+(t/10)%10);
  statusChar(18,'0'+t%10);
  statusChar(19,'b');

// display position on page (as scroll bar)
  statusChar(lastPercent,' ');
  lastPercent = (cursor*30)/t+33;
  if (lastPercent>63) lastPercent=63;
  statusChar(32,'|');
  statusChar(63,'|');
  statusChar(lastPercent,'*');

// count words and write
  w=0;
  for(t2=1;t2<t;t2++)
  {
     if (isSpaceAt(t2) && !isSpaceAt(t2-1)) w++;
  }

statusChar(21,'0'+(w/1000)%10);
  statusChar(22,'0'+(w/100)%10);
  statusChar(23,'0'+(w/10)%10);
  statusChar(24,'0'+w%10);
statusChar(25,'w');

// read battery, display battery, disable ADC
//  ADCSRA |= (1 << ADSC);
  while (ADCSRA & (1 << ADSC));
  low = ADCL;
  high = ADCH;
  //return (float)((high << 8) + low) * 1.6 / 1024 / 0.26666;
  
  //v=(((high << 8) + low)*6000)/1024;
  adc=(float)((high << 8) + low) * 1.6 / 1024 / 0.26666;
  v=adc*1000;
  
  statusChar(0,'B');
  statusChar(2,'0'+(v/1000)%10);
  statusChar(3,'.');
  statusChar(4,'0'+(v/100)%10);
  statusChar(5,'0'+(v/10)%10);
  //statusChar(6,'0'+v%10);
  statusChar(6,'V');
}

char gotoPrevPage()
{
	if (pageCurrent>0)
	 { 
		 if (pageEdited) pageSave();	 
		 pageCurrent--;
		 cursor=0;
		 topScreen=0;
		 pageReload();
		 return 1; 
	}
	return 0;
}	

char gotoNextPage()
{
	if (pageCurrent<31)
	 { 
		 if (pageEdited) pageSave();		
		 pageCurrent++;
		 cursor=0;
		 topScreen=0;
		 pageReload();
		 return 1; 
	}
	return 0;
}	

void clearPage()
{
  pageEdited = 1;
  memset(pageBuffer,0,4096);	
  cursor=0;
  topScreen=0;
}



char insertAt(int pos, char c)
{
  char t;
  if ((pageBuffer[onePage-1]==0) || (pageBuffer[onePage-1]==' '))  
  {
    while(pos<onePage)
      {
	t=pageBuffer[pos];
	pageBuffer[pos++]=c;
	c=t;
      }	
    return 1;
  }
  else
  {
    pageBuffer[pos]=c;
    return 0;
  }
}


//  0 x y m 0 z a' 0 b c
// zA		col=1
// bc
//
//  0 x y m 0 z a 0' b c
// za'	col=2
// bc
//
//  0 x y m 0 z a 0 b' c
// za  col=0
// Bc
//



void deleteAt(int pos)
{
  while(pos<onePage)
  {
    pageBuffer[pos]=pageBuffer[pos+1];
    pos++;
  }
  pageBuffer[onePage-1]=0;
}

int cHome()
{
  int col=0;
  while (cursor && pageBuffer[cursor-1]) { cursor--; col++; }
  return col;
}
void cEnd()
{ // goes to first character in next line.
  while(pageBuffer[cursor++]);
}
void cRight(int n)
{
  if (pageBuffer[cursor]) { while(n && pageBuffer[cursor+1]) {cursor++; n--;} }
}

//jjjj0
//0
//xxX0
//



void cursorUp()
{
  int col;
  int prev=cursor;
  col = cHome();
  if (cursor) 
  {
    cursor--; 
    cHome();
    cRight(col);
  }
  
  prev=prev-32;
  if (prev>cursor) cursor=prev;
}
    
void cursorDn()
{
  int col;
  int prev=cursor;
  col = cHome();
  cEnd();
  cRight(col);
  
  prev=prev+32;
  if (prev<cursor) cursor=prev;
  if (cursor>onePage) cursor=onePage-1;
}  


void cls()
{
  SRXEFill(0);
  memset(screenBuff,0,oneScreen);
}

static const uint16_t QR_BYTE_SIZE[30] =
 { //capacity in bytes - by 'version'
  0, // 0
  17,    32,    53,    78,    106,
  134,   154,   192,   230,   271, //10
  321,   367,   425,   458,   520, //15
  586,   644,   718,   792,   858, //20
  929,   1003,  1091,  1171,  1273, //25
  1367,  1465,  1528,  1628 //29    
  };


int QR_bytes(int version)
{
  if (version<0) return 0;
  if (version>29) return 1;

  return QR_BYTE_SIZE[version];
}

      
char QR_export(uint8_t *data, int amount, int version,  int xpos, bool waitBefore)
{
 // takes data from *data, generates QR code sized based on version
 // and draws it on screen at xpos - xpos+size (y =0 through size) 
 
 QRCode qrcode;
 uint8_t qrcodeData[qrcode_getBufferSize(29)];
 uint8_t size =version*4+29;
 uint8_t sliver[ 136 ];
 uint8_t scale=1;
 uint8_t key;
 
 int x,y;
  
  if ((amount==0) || (amount>QR_bytes(version))) amount = QR_bytes(version);
  
  qrcode_initBytes(&qrcode,qrcodeData,version,0,data,amount);
  
  key=0;
  if (waitBefore) 
  {
    while (key==0) 
    {
      SRXEdelay(10);
      key=SRXEGetKey();         
    }
  }  
    
    
  scale=1;
  if (version<=12) scale = 2;
  if (version<=7) scale = 3;
  
  for(x=(xpos/3)*3;x<xpos+size*scale;x+=3)
    {
      for(y=0;y<136;y++)
      {
	sliver[y]=0;
	if (qrcode_getModule(&qrcode, (x-xpos)/scale, y/scale)) sliver[y] |= 0xe0; // 3 bits
	if (qrcode_getModule(&qrcode, (x-xpos +1)/scale, y/scale)) sliver[y] |= 0x1c; // 3 bits
	if (qrcode_getModule(&qrcode, (x-xpos +2)/scale, y/scale)) sliver[y] |= 0x03;	 //2 bits
      }
      
      SRXESetPosition(x, 0, 3, 136);
      SRXEWriteDataBlock(sliver, 136); 
   }
}


void swapInNs()  
{
 int t=onePage-1;
 while( (t) && (pageBuffer[t]==0)) t--;
 while(t) {
  if (pageBuffer[t]==0) pageBuffer[t]='\n';
  t--;
  }
}

void swapOutNs()
{
 int t=onePage-1;
 while(t) {
  if (pageBuffer[t]=='\n') pageBuffer[t]=0;
  t--;
  }
}


int getPageEnd()
{
 int t=onePage;
 while( (t) && (pageBuffer[--t]==0));
 return ++t;
}

void exportAsQRs()
{
  int pSize;
  int t;
  pSize=getPageEnd()+1;
  swapInNs();
  
  cls();
  
  for(t=6;t<=29;t++) 
    {
      if (QR_bytes(t)>=pSize)
      {
	QR_export(pageBuffer,pSize,t,120,false);
	while(SRXEGetKey()==0);
	cls();
	swapOutNs();
	return;
      }
    }

  for(t=6;t<=29;t++) 
    {
      if (2*QR_bytes(t)>=pSize)
      {
	QR_export(pageBuffer,0,t,12,false);
	QR_export(pageBuffer+QR_bytes(t),pSize-QR_bytes(t),t,240,false);
	
	while(SRXEGetKey()==0);
	cls();
	swapOutNs();
	return;
      }
    }    
  
  for(t=6;t<=29;t++) 
    {
      // there's 3257-4096 bytes... (or 16 less?
      // just need 848 in the first code...
      // x  in the middle
      // and 1628 in the last.
      // this makes sure the last code completely covers the first.
      
      
      
      if (QR_bytes(t)>=pSize-QR_bytes(29)-QR_bytes(20))
      {
	
	QR_export(pageBuffer,0,t,12,false);
    	QR_export(pageBuffer+QR_bytes(t),0,20,240,false);


	if (QR_export(pageBuffer+QR_bytes(t)+QR_bytes(20),pSize-QR_bytes(t)-QR_bytes(20),29,12,true) == ' ') while(SRXEGetKey()==0);
	cls();
	swapOutNs();
	return;
      }
    }      
    
  
}

int main(void) {
 
  byte key, deleteCount, undoCount; 
   
  uint16_t autoPowerTime;
  
  TRXPR = 1 << SLPTR; // send transceiver to sleep
  
  SRXEInit(0xe7, 0xd6, 0xa2); // initialize display
  memset(screenBuff,0,oneScreen);
  

  pageCurrent = 0;
  pageReload();
  SRXEdelay(1000); // give some time for CS chip to stabilize...
  pageReload();
  
  cursor=0;
  
  screenUpdate();
  autoPowerTime=0;
  deleteCount=0;
  undoCount=0;
  statusBarUpdate();
  
  while (1)
  {
    key=SRXEGetKey();   
	  
    if (key == 0)	    
    {
      SRXEdelay(10);
      
      autoPowerTime++;
      if (XgetPowerKey()) powerTime++;
	    else powerTime = 0; 
      
      if (autoPowerTime == 100) statusBarUpdate();
      
      if ((powerTime>300) || (autoPowerTime>30000))
	{
		pageSave();
		//while(XgetPowerKey()) SRXEdelay(1);
		  SRXEdelay(10);
		
		SRXESleep();
	      
		memset(screenBuff,0,oneScreen);
		//XsetupCounter2();
		//screenUpdate();		
		autoPowerTime=0;
		powerTime = 0;
	}
     } else { // key!=0
	autoPowerTime = 0;  
	if (key!=K_F6)	deleteCount = 0;	
	if (key!=K_F7)	undoCount = 0;	
        }
     blinkTime++;

  switch(key)
  {
	case K_NONE: break; // timeout
    case K_up: cursorUp(); break;
    case K_down: cursorDn();
    
     break;
    case K_left: cursor--; break;
    case K_right: cursor++; break;
    case K_del: 
		if (cursor>0) cursor--;
	        deleteAt(cursor);	
        break;
    case K_backspace:
        deleteAt(cursor);	
          break;
	      
    case K_F1: pageSave(); break;
    case K_F5: gotoPrevPage();
       break;
    case K_F10: gotoNextPage();
       break;
    case K_F6: if (++deleteCount > 2) { clearPage(); }
       break;
    case K_F7: if (++undoCount > 2) { pageReload(); }
       break;
    
    case K_F8: // export
       exportAsQRs();
      break;
/*    case K_F2: // export 1 big qr code
      cls();
      QR_export(pageBuffer, 0, 29, 120,false); // export as version 29: 133x133 pixels, low redundancy -> 1628 bajtów... takes 22secs...
      while(!SRXEGetKey());
      cls();
      break; // contrast up
    case K_F3: // export 1 small qr code
      cls();
      QR_export(pageBuffer, 0, 12, 120,false); // export as version 12: 65*65 , 367 bajtów. ok for upscaling*2... takes 5 secs...
      while(!SRXEGetKey());
      cls();
      break; // contrast down*/ 
    case K_F2: SRXEIncreaseVop(); break;
    case K_F3: SRXEDecreaseVop(); break;
    case K_F4: // export 2 big QR codes
      cls();
      QR_export(pageBuffer,0, 29, 10,false); // export as version 29: 133x133 pixels, low redundancy -> 1628 bajtów...
      QR_export(pageBuffer+1628,0 ,29, 230,false); // export as version 29: 133x133 pixels, low redundancy -> 1628 bajtów...
      
      while(!SRXEGetKey());
      cls();
      break; 

    case K_pgUp: cursorUp();cursorUp();cursorUp();cursorUp();cursorUp(); break;
    case  K_pgDown: cursorDn();cursorDn();cursorDn();cursorDn();cursorDn(); break;
    case  K_home: cursor=0; topScreen=0; break;
    case  K_end: cursor = getPageEnd(); break;
    
    case K_F9:
	      SRXESleep(); 
	      memset(screenBuff,0,oneScreen);
    case K_menu: break;
    case K_enter: 
       insertAt(cursor++,0);
       break;

    default:        
	insertAt(cursor++,key);
        pageEdited = 1;
  }
  if (cursor<0) { cursor=0; }
  if (cursor>=onePage) { cursor=onePage-1; }

  pageBuffer[onePage]=0; // guardian for some searches, possibly obsolete.
  
  //if (menuTime<200) { 
  screenUpdate();
	   //}
	   
  }	    // while(1)- main loop
}



