  #include <Arduino.h>
  #include <Stepper.h>
  #include "Pulsante.h"
  #include "MyTimer.h"
  #include <Wire.h> //sda ed scl implicitamente collegati ad a4 ed a5
  #include "SoftwareSerial.h" //servira per il collegamento all'mp3 player
  #include <Servo.h> //Servo motore
  #include <DFRobotDFPlayerMini.h>
  //utilizzate queste librerie perchè più leggere della Adafruit per l'utilizzo che ne viene fatto
  #include "SSD1306Ascii.h"
  #include "SSD1306AsciiAvrI2c.h"

  #define DS3231_I2C_ADDRESS 0x68 //address of DS3231 module
  #define DS3231_EEPROM_ADDRESS 0x57 //address of DS3231 EEPROM module

  #define pul1 6
  #define pul2 7
  #define pul3 4
  #define servoPin 13
  #define pinLuce 2

  SSD1306AsciiAvrI2c display;
  byte volume=28;

  SoftwareSerial ss(3, 2); //RX, TX
  DFRobotDFPlayerMini mp3;

  const int stepsPerRevolution = 2048;
  const int    stepsPerRev  = 2048; // for the 28BYJ-48 motor
  const double step60Actual = stepsPerRev / 60.0;  // 2048/60 = 34.13333
  const int    step60       = int(floor(step60Actual)); // 34
  const double step12Actual = stepsPerRev / 12.0;  // 2048/12 = 170.66667
  const int    step12       = int(floor(step12Actual)); // 170
  const int    direction    = -1;  // set to -1 to go counter-clockwise
  double sDiscrep = 0.0;
  double mDiscrep = 0.0;
  double hDiscrep = 0.0;
  bool startMov=false;
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year; //global variables
  byte minuteprev=99;

  char datastr[2];
  unsigned long tlamp;
  bool mostraCifra=false;
  bool cambiaCifra=false;
  unsigned int rlamp=300;

  //variabili per date cambio ora solare/legale
  uint16_t doy;
  uint16_t sday;
  uint16_t lday;  

  MyTimer tm(1000);     //imposto un timer per lo spostamento della lancetta
  MyTimer tcucu(2000); //timer per rintocchi del cuccu ogni 2 sec

  Servo sc;
  bool openBird=false;
  bool closedCucu=false;

  //variabili per cucu
  bool startCucu=false;
  bool endCucu=true;
  byte numeroCucu;
  byte rintocchi;
  bool statoCucu=true;

  //definizione pulsanti
  Pulsante p1(pul1, 3);
  Pulsante p2(pul2, 3);
  Pulsante p3(pul3, 3); 

  bool lp1=false;
  bool lp2=false;
  bool lp3=false;
  bool modImpo=false;

  byte pressP1=0;
  byte pressP2=0;
  byte pressP3=0;
  byte menuImpo=0;

  //12 ore corrispondo a 12 x 60 = 720 minuti
  int countMin=0;
  int calcMin=0;
  int currMin=0;

  Stepper myStepper(stepsPerRevolution, 8, 10, 9, 11);

  //prottipi delle funzioni
  void writeIntEEPROM(int);
  void displayOled(bool);
  byte readEEPROM(int, int);
  void writeEEPROM(int,byte,int);

  uint16_t getDayOfYear( const uint8_t, const uint8_t, const uint16_t);
  uint8_t getLastSundayInMonth( const uint8_t , const uint16_t);

  //function per trattamento delle date

  uint8_t getWeekDay( uint8_t day, uint8_t month, uint16_t year )
  {
    //utilizzato l'algoritmo di Zeller
    if ( month <= 2 )
    {
      month += 12;
      --year;
    }

    uint8_t j = year % 100;
    uint8_t e = year / 100;
    return ( ( ( day + ( ( ( month + 1 ) * 26 ) / 10 ) + j + ( j / 4 ) + ( e / 4 ) - ( 2 * e ) ) - 2 ) % 7 );
  }

  uint8_t getDaysInMonth (const uint8_t, const uint16_t);

  uint8_t getLastSundayInMonth( const uint8_t month, const uint16_t year )
  {
    uint8_t d = getDaysInMonth( month, year );
    while ( getWeekDay( --d, month, year ) != 6 );
    return d;
  }

  uint16_t getDayOfYear( const uint8_t day, const uint8_t month, const uint16_t year )
  {
    uint16_t d = day;
    uint8_t m = month;
    while ( --m ) d += getDaysInMonth( m, year );
    return d;
  }

  bool isLeapYear( const uint16_t year )
  {
    return ( (year % 400 == 0) || (year % 4 == 0 && year % 100 != 0) );
  }

  uint16_t getDaysInYear( const uint16_t year )
  {
    return isLeapYear( year ) ? 366 : 365;
  }

  /*
  uint16_t getNextLeapYear( const uint16_t year )
  {
    uint16_t y = year;
    while ( !isLeapYear( ++y ) );
    return y;
  }
  */
  /*
  uint16_t getPreviousLeapYear( const uint16_t year )
  {
    uint16_t y = year;
    while ( !isLeapYear( --y ) );
    return y;
  }
  */
  uint8_t getDaysInMonth( const uint8_t month, const uint16_t year )
  {
    if ( month == 2 )  
      return isLeapYear( year ) ? 29 : 28;
      
    else if ( month == 4 || month == 6 || month == 9 || month == 11 )  
        return 30;
        
    return 31;
  }

  //conversion Dec to BCD 
  byte decToBcd(byte val) {
    return((val / 10 * 16) + (val % 10));
  }

  //conversion BCD to Dec 
  byte bcdToDec(byte val) {
    return((val / 16 * 10) + (val % 16));
  }

  void SetRtc(byte second, byte minute, byte hour, byte dayOfWeek, byte dayOfMonth, byte month, byte year) {
    Wire.beginTransmission(DS3231_I2C_ADDRESS);
    Wire.write(0); //set 0 to first register

    Wire.write(decToBcd(second)); //set second
    Wire.write(decToBcd(minute)); //set minutes 
    Wire.write(decToBcd(hour)); //set hours
    Wire.write(decToBcd(dayOfWeek)); //set day of week (1=su, 2=mo, 3=tu) 
    Wire.write(decToBcd(dayOfMonth)); //set day of month
    Wire.write(decToBcd(month)); //set month
    Wire.write(decToBcd(year)); //set year
    Wire.endTransmission();
  }

  //read RTC
  void GetRtc() {
    Wire.beginTransmission(DS3231_I2C_ADDRESS);
    Wire.write(0); //write "0"
    Wire.endTransmission();

    Wire.requestFrom(DS3231_I2C_ADDRESS, 7);	//request - 7 bytes from RTC
    second = bcdToDec(Wire.read() & 0x7f);
    minute = bcdToDec(Wire.read());
    hour = bcdToDec(Wire.read() & 0x3f);

    dayOfWeek = bcdToDec(Wire.read());
    dayOfMonth = bcdToDec(Wire.read());
    month = bcdToDec(Wire.read());
    year = bcdToDec(Wire.read());
  }

  void muoviLancetta() {

    if (modImpo)
      return;
    
    GetRtc();

    if (minute==minuteprev)
      return;

    minuteprev=minute;

    mDiscrep += (step60Actual - step60);
    if (mDiscrep > 1.0) {
        myStepper.step((step60 + 1) * direction);
        mDiscrep -= 1.0;
        countMin++;
    } 
    else {
        countMin++;
        myStepper.step(step60 * direction);
  }

    if (hour >= 12)
        calcMin = ((hour - 12) * 60) + minute;
    else   
      calcMin = (hour * 60) + minute;

    if (countMin==720)
        countMin = 0;

    writeIntEEPROM(countMin);
    
    displayOled(false);

    //test per cambio ora legale // solare viene eseguito un reset con il nuovo orario
    if (hour==2) {
      if (minute==0) {
           if (doy==lday) {
               hour++;
               SetRtc(0, minute, hour, dayOfWeek, dayOfMonth,month, year);               	
               //reset scheda
               delay(100);
               asm volatile (" nop "); // pausa di 62nSec
               asm volatile (" jmp 0 "); //reset impostando il program counter all'indirizzo 0
           }
           else 
           if (doy==sday) {
              byte cambioOra = readEEPROM(2, DS3231_EEPROM_ADDRESS);
              if (cambioOra==1) {
                writeEEPROM(2, 0, DS3231_EEPROM_ADDRESS);
              }
              else {
                 hour--;
                 writeEEPROM(2, 1, DS3231_EEPROM_ADDRESS);
                 SetRtc(0, minute, hour, dayOfWeek, dayOfMonth,month, year);	
                 delay(100);
                 asm volatile (" nop "); // pausa di 62nSec
                 asm volatile (" jmp 0 "); //reset impostando il program counter all'indirizzo 0
              }
           }
      }
    }
    
  }

  void muoviUnoStep(int dir) {

    if (!modImpo)
        return;
    
  // dir = dir * -1;

    mDiscrep += (step60Actual - step60);
    if (mDiscrep > 1.0) {
        myStepper.step((step60 + 1) * dir);
        mDiscrep -= 1.0;
    } 
    else {    
        myStepper.step(step60 * dir);
    }

    if (dir == -1)
        countMin++;
    else
        countMin--;   
    
    if (countMin==720)
        countMin = 0;
    else
    if (countMin<0)
        countMin=719;
    //EEPROM.write(0, countMin);    
    
  }

  void testPressedButton() {
    
    //la funzione released() restituisce al rilascio del pulsante i valori: 
    //0 - tasto non premuto
    //1 - tasto premuto
    //2 - pressione prolungata del tasto
    
    pressP1=p1.released();
    pressP2=p2.released();
    pressP3=p3.released();

    if (pressP1>=1) 
        lp1=true;

    if (pressP2>=1) 
        lp2=true;

    if (pressP3>=1) 
        lp3=true;

      //menuImpo = 0 Regolazione veloce lancette
      //menuImpo = 1 Regolazione by step lancette
      //menuImpo = 2 Regolazione ore
      //menuImpo = 3 Regolazione minuti
      //menuImpo = 4 Regolazione data aa
      //menuImpo = 5 Regolazione data mm
      //menuImpo = 6 Regolazione data gg
      
      if (lp1) {
          lp1=false;
          lp2=false;
          if (!modImpo) {
            modImpo=true;
            menuImpo=0;
            display.clear();
            display.setCursor(20,2);
            display.print("...Zero set...");           
            display.setCursor(20,3);
            display.print("...Fast move...");           
            display.setCursor(10,5);
            display.print("Press P2 to start");
            display.setCursor(10,6);
            display.print("again P2 to stop");
          }
          else {
            menuImpo++;
            if (menuImpo==1) {
                display.setCursor(20,3);
                display.print("...Fine move...");           
                display.setCursor(10,5);
                display.print("Press P2 to step ");
                display.setCursor(10,6);
                display.print("                ");
            }          
            if (menuImpo>=2 && menuImpo < 8) {
              if (menuImpo==2) {
                  display.clear();
              }
              displayOled(false);
              tlamp=0;
              mostraCifra=false;
              cambiaCifra=false;
            }          
            
            if (menuImpo==8) {
              countMin=0;
              menuImpo=0;
              writeIntEEPROM(countMin);
              delay(50);
              writeEEPROM(4,volume, DS3231_EEPROM_ADDRESS);
              int dow;
              dow=getWeekDay(dayOfMonth,month,year);
              //getWeekDay ritorna 6=dom,0=lun, 1=mar,....5=sab, quindi rimappoi giorni per l'rtc in cui 1=dom, 2=lun...
              if (dow==6)
                  dow=1;
              else 
                  dow+=2;                   
              SetRtc(0, minute, hour, dow, dayOfMonth,month, year);	
              modImpo=false;
              GetRtc();
              minuteprev=minute;
              delay(200);
              display.clear();
              //software reset
              asm volatile (" nop "); // pausa di 62nSec
              asm volatile (" jmp 0 "); //reset impostando il program counter all'indirizzo 0
            }
          }
        return;
        }
    
    //menuImpo = 0 - set lancette sullo zero - Movimento continuo
    //menuImpo = 1 - set Orario corrente - Movemento by step
    //menuImpo = 2 - Regolazione ore
    //menuImpo = 3 - Regolazione minuti
    //menuImpo = 4 - Regolazione data aa
    //menuImpo = 5 - Regolazione data mm
    //menuImpo = 6 - Regolazione data gg
    //menuImpo = 7 - Regolazione volume mp3 player

    if  (modImpo==true) {
        if (menuImpo==0) {

          if (pressP2>=1)
              startMov=!startMov;
          if (startMov)
              muoviUnoStep(-1);                        
        }   
        else   
        if (menuImpo==1) {
          if (pressP2>=1)
              muoviUnoStep(-1);                        
        }
        else 
        if (menuImpo==2) {
            if (pressP2>=1) {
              hour++;
              if (hour>23)
                  hour=0;
            }
        
            if ((millis() - tlamp) > rlamp) {
                mostraCifra=!mostraCifra;  
                tlamp=millis();
                cambiaCifra=true;
            }

            if (cambiaCifra) {
                cambiaCifra=false;            
                if (mostraCifra) {
                      if (hour>9) {                       
                          display.setCursor(0,4);            
                          display.print(hour);                  
                      } 
                      else {                    
                          display.setCursor(0,4);            
                          display.print("0");                  
                          display.setCursor(0+6,4);            
                          display.print(hour);                  
                      }
                }    
                else {
                    display.setCursor(0,4);
                    display.print("  ");
                }   
            } 
        }  
        else 
        if (menuImpo==3) {
            if (pressP2>=1) {
              minute++;
              if (minute>59)
                  minute=0;
            }
        
            if ((millis() - tlamp) > rlamp) {
                mostraCifra=!mostraCifra;  
                tlamp=millis();
                cambiaCifra=true;
            }

            if (cambiaCifra) {
                cambiaCifra=false;            
                if (mostraCifra) {
                      if (minute>9) {                       
                          display.setCursor(22,4);            
                          display.print(minute);                  
                      } 
                      else {                    
                          display.setCursor(22,4);            
                          display.print("0");                  
                          display.setCursor(22+6,4);            
                          display.print(minute);                  
                      }
                }    
                else {
                    display.setCursor(22,4);
                    display.print("  ");
                }   
            } 
        }   
        else 
        if (menuImpo==4) {
            if (pressP2>=1) {
              year++;
              if (year>50)
                  year=23;
            }
        
            if ((millis() - tlamp) > rlamp) {
                mostraCifra=!mostraCifra;  
                tlamp=millis();
                cambiaCifra=true;
            }

            if (cambiaCifra) {
                cambiaCifra=false;            
                if (mostraCifra) {
                    display.setCursor(44,6);            
                    display.print(year);                  
                }    
                else {
                    display.setCursor(44,6);
                    display.print("  ");
                }   
            } 
        }   
        else 
        if (menuImpo==5) {
          if (pressP2>=1) {
            month++;
            if (month>12)
                month=1;
          }
        
            if ((millis() - tlamp) > rlamp) {
                mostraCifra=!mostraCifra;  
                tlamp=millis();
                cambiaCifra=true;
            }

            if (cambiaCifra) {
                cambiaCifra=false;            
                if (mostraCifra) {
                      if (month>9) {                       
                          display.setCursor(22,6);            
                          display.print(month);                  
                      } 
                      else {                    
                          display.setCursor(22,6);            
                          display.print("0");                  
                          display.setCursor(22+6,6);            
                          display.print(month);                  
                      }
                }    
                else {
                    display.setCursor(22,6);
                    display.print("  ");
                }   
            } 
        }   
        else 
        if (menuImpo==6) {
            if (pressP2>=1) {
            dayOfMonth++;          
            if (dayOfMonth>getDaysInMonth(month, year))
                dayOfMonth=1;
            }
        
            if ((millis() - tlamp) > rlamp) {
                mostraCifra=!mostraCifra;  
                tlamp=millis();
                cambiaCifra=true;
            }

            if (cambiaCifra) {
                cambiaCifra=false;            
                if (mostraCifra) {
                      if (dayOfMonth>9) {                       
                          display.setCursor(0,6);            
                          display.print(dayOfMonth);                  
                      } 
                      else {                    
                          display.setCursor(0,6);            
                          display.print("0");                  
                          display.setCursor(0+6,6);            
                          display.print(dayOfMonth);                  
                      }
                }    
                else {
                    display.setCursor(0,6);
                    display.print("  ");
                }   
            } 
        }   
        else 
        if (menuImpo==7) {
            if (pressP2>=1) {
            volume+=2;          
            if (volume>30)
                volume=0;
            }
        
            if ((millis() - tlamp) > rlamp) {
                mostraCifra=!mostraCifra;  
                tlamp=millis();
                cambiaCifra=true;
            }

            if (cambiaCifra) {
                cambiaCifra=false;            
                if (mostraCifra) {
                      if (volume>9) {                       
                          display.setCursor(65,2);            
                          display.print(volume);                  
                      } 
                      else {                    
                          display.setCursor(65,2);            
                          display.print("0");                  
                          display.setCursor(65+6,2);            
                          display.print(volume);                  
                      }
                }    
                else {
                    display.setCursor(65,2);
                    display.print("  ");
                }   
            } 
        }   


    }

    //Pressione P3 per attivazione/disattivazione cucu
    if (!modImpo) {
      if (lp3) {
        lp3=false;
        byte  EEstatoCucu = readEEPROM(3, DS3231_EEPROM_ADDRESS);
        if (EEstatoCucu != 1)
           writeEEPROM(3, 1, DS3231_EEPROM_ADDRESS);
        else
           writeEEPROM(3, 0, DS3231_EEPROM_ADDRESS);            
        delay(50);
        displayOled(false);   
      }
    }  
  }

  void writeEEPROM(int address, byte valore, int i2c_address){
    // Begin transmission to I2C EEPROM
    Wire.beginTransmission(i2c_address);
  
    // Send memory address as two 8-bit bytes
    Wire.write((int)(address >> 8));   // MSB
    Wire.write((int)(address & 0xFF)); // LSB
  
    // Send data to be stored
    Wire.write(valore);
  
    // End the transmission
    Wire.endTransmission();
  
    // Add 5ms delay for EEPROM
    delay(5);
  }

  //scrittura su EEPROM di DS3231 di un intero. Spazio occupato 2 byte
  void writeIntEEPROM (int valore) {
    byte lbyte = lowByte(valore);
    byte hbyte = highByte(valore);

    writeEEPROM(0, lbyte, DS3231_EEPROM_ADDRESS);
    writeEEPROM(1, hbyte, DS3231_EEPROM_ADDRESS);   
  }

  byte readEEPROM(int address, int i2c_address){
    // Define byte for received data
    byte rcvData = 0x00;
  
    // Begin transmission to I2C EEPROM
    Wire.beginTransmission(i2c_address);
  
    // Send memory address as two 8-bit bytes
    Wire.write((int)(address >> 8));   // MSB
    Wire.write((int)(address & 0xFF)); // LSB
  
    // End the transmission
    Wire.endTransmission();
  
    // Request one byte of data at current memory address
    Wire.requestFrom(i2c_address, 1);
  
    // Read the data and assign to variable
    rcvData =  Wire.read();
  
    // Return the data as function output
    return rcvData;
  }

  int readIntEEPROM () {

    byte rintl = readEEPROM(0, DS3231_EEPROM_ADDRESS);
    byte rinth = readEEPROM(1, DS3231_EEPROM_ADDRESS);

    int rint = (rinth*256) + rintl;
    
    return rint;
  }

  void sincOra() {

    modImpo=true;

    GetRtc();
    
    if  (hour >= 12)
        currMin = ((hour - 12)*60) + minute;
    else  
        currMin = (hour * 60) + minute;

    displayOled(true);

    while (currMin != countMin)  {
      muoviUnoStep(-1);
    }
    
    modImpo=false;
  }

  void displayOled(bool dispSinc) {

    display.setCursor(0,0);
    display.print(countMin);
    
    if  (hour >= 12)
        currMin = ((hour - 12)*60) + minute;
    else  
        currMin = (hour * 60) + minute;
    
    display.setCursor(0,2);
    display.print(currMin);

    display.setCursor(0,4);
    char ostr[2];
    char dstr[3];
    sprintf(ostr, "%02d", hour);
    display.print(ostr);
    display.setCursor(14,4);
    display.print(":");
    display.setCursor(22,4);
    sprintf(ostr, "%02d", minute);
    display.print(ostr);

    if  (dispSinc) {
        display.setCursor(35,0);
        display.print("..Sincro..");

    }  
    else {
      //display data
        display.setCursor(0,6);
        sprintf(ostr, "%02d", dayOfMonth);
        display.print(ostr);
        display.setCursor(15,6);
        display.print("/");
        display.setCursor(22,6);
        sprintf(ostr, "%02d", month);
        display.print(ostr);
        display.setCursor(37,6);    
        display.print("/");
        display.setCursor(44,6);
        display.print(year);

        doy = getDayOfYear(dayOfMonth,month,year + 2000);
        display.setCursor(90, 2);
        sprintf(dstr, "%03d", doy);
        display.print(dstr); 

        //uint8_t legalDay = getLastSundayInMonth (3, year);
        //uint8_t solarDay = getLastSundayInMonth (10, year);

        display.setCursor(90, 4);
        lday = getDayOfYear(getLastSundayInMonth (3, year + 2000),3,year + 2000);
        sprintf(dstr, "%03d", lday);
        display.print(dstr); 

        display.setCursor(90, 6);
        sday = getDayOfYear(getLastSundayInMonth (10, year + 2000),10,year + 2000);
        sprintf(dstr, "%03d", sday);
        display.print(dstr);       

        //display intensità luce. Con fotoresistore 5549 luce intensa > 600, mentre quasi buio <= 5
        display.setCursor(90,0);
        display.print("    ");
        display.setCursor(90,0);
        char sluce[3];
        sprintf(sluce, "%03d", analogRead(pinLuce));
        display.print(sluce);

        //display stato cucu attivo=1 non attivo 0
        display.setCursor(35,0);
        display.print("        ");
        display.setCursor(35,0);
        byte EEstatoCucu = readEEPROM(3, DS3231_EEPROM_ADDRESS);
        if (EEstatoCucu != 1)         
           display.print("Cucu off");
        else
           display.print("Cucu on ");

        //display volume 
        display.setCursor(35,2);
        display.print("        ");
        display.setCursor(35,2);
        byte EEvolume = readEEPROM(4, DS3231_EEPROM_ADDRESS);
        display.print("Vol. ");
        char svol[2];
        sprintf(svol, "%02d", EEvolume);
        display.print(svol);
   }
  }
  void openCucu() {
    sc.write(180);
    delay(1000);
  }

  void closeCucu() {

    if (closedCucu)
      return;

    closedCucu=true;
    sc.write(0);
    delay(1000);
  }


  void testCucu() {

    if (!modImpo) {
        //if ((minute==0 || ((minute % 5) == 0)) && hour>=7 && hour<=21) {
        if (minute==0 && hour>=7 && hour<=21) {
          //controllo luminosità
            if (!startCucu ) { 
            //controllo statoCucu
            // statoCucu = off se premuto tasto di disattivazione P3 oppure per luce non sufficiente
            //Lettura eprom del ds3231 per stato del cucu (cambia alla pressione di P3)
            byte EEstatoCucu = readEEPROM(3, DS3231_EEPROM_ADDRESS);
            if (EEstatoCucu != 1)
                statoCucu=false;
            else
                statoCucu=true;

            //Verifica intensità della luce. Se inferiore a 250 per fotoresistore 5506 non attivo il cucu
            if (statoCucu==true) {
              unsigned int iluce=analogRead(pinLuce);
              if (iluce<=250) {
                statoCucu=false;
              }
            }    

            if (statoCucu==true) {
              startCucu=true;
              endCucu=false;
              closedCucu=false;
              openCucu();
              if (hour > 12) {
                  numeroCucu = hour - 12;
              }
              else {     
                  numeroCucu = hour;
              }   
              rintocchi=0;
          }
          } 
      }
    }

    if (!modImpo) {
        if (endCucu && startCucu) {
            closeCucu();
        }
    } 

    if (!modImpo) {
          if (minute==01) {
            if (endCucu && startCucu)
              startCucu = false;    
        }
    } 
    //i rintocchi del cucu vengono fatti attraverso il timer tcucu (impostato a 2 sec)
  }

  //callback richiamata dal timer del cuccu ogni 2 secondi
  void playCucu() {

    if (!startCucu)
      return;
    
    if (rintocchi==numeroCucu) {
      endCucu=true;
      return;
    }

    rintocchi++;
    mp3.play(0);

  }

  void setup() {
    //Serial.begin(9600);
    myStepper.setSpeed(5);

    delay(1000);

    display.begin(&Adafruit128x64, 0x3C);
    //display.setFont(Callibri14);
    display.setFont(Adafruit5x7);
    display.clear();
    display.setCursor(4, 3);
    display.print("Stepper clock ver 1.1");
    display.setCursor(10, 5);
    display.print("...init routine...");

    delay(2000);
    ss.begin(9600);
    delay(1000);

  if (!mp3.begin(ss)) {
      for(;;);  
    }

    sc.attach(servoPin);
    sc.write(0);
    delay(1000);

    //inizializzazione mp3 player. Il volume viene salvato all'indirizzo 4 della EEPROM del DS3231
    byte EEvolume = readEEPROM(4, DS3231_EEPROM_ADDRESS);
    if (EEvolume > 30) {
       EEvolume = 26;
       writeEEPROM(4, EEvolume, DS3231_EEPROM_ADDRESS);
    }

    volume=EEvolume;
    mp3.volume(EEvolume);
    mp3.play(0);

    //imposto timer per visualizzazione delle cifre dell'ora
    tm.init();
    //impostazione callback a scatto del timer  per visualizzazione dell'ora
    tm.cback(muoviLancetta);  

  //imposto timer per cucu
    tcucu.init();
    //imposto callback per scatto del cucu ogni 2 secondi
    tcucu.cback(playCucu);

    Wire.begin(); //start I2C communication

    delay(1000);

    //SetRtc(0, 38, 15, 6, 02, 6, 23);	//sec, min, hour, dayOfWeek (sun,mon, tue,...), dayOfMonth, month, year  
    
    //vengono eseguite 3 sincronizzazioni tra il modulo RTC ed il meccanismo
    //la prima più grossolona che può durare anche alcuni minuti le successive sempre più raffinat   
    countMin = readIntEEPROM();

    display.clear();
    sincOra();
    sincOra();
    sincOra();

    minuteprev = minute;

    writeIntEEPROM(currMin);
    
    display.clear();
    displayOled(false);
  }

  void loop() {

    //verifica pressione tasti
    testPressedButton();  
    
    //timer per spostamento lancetta
    tm.run();

    //timer per cuccu
    tcucu.run();

    //verifica se azionamento cucu
    testCucu();

  }

