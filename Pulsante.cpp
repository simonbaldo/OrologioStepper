#include "Pulsante.h"

Pulsante::Pulsante(uint8_t pinpul, unsigned long secP){
    secPress=(unsigned long)secP * 1000;
    pinPulsante=pinpul;
    pinMode(pinPulsante, INPUT);
    pulPressed=false;
    pulReleased=false;
    preState=LOW;
    curState=LOW;
};

bool Pulsante::pressed() {
   if (pulPressed) {
       if ((millis() - tPressed) > debounceDelay) {
          pulPressed=false;
          return true;
       }
   }
   else {
       if  (digitalRead(pinPulsante)) {
            pulPressed=true;
            tPressed=millis();            
        }
   }
   return false;             
};

uint8_t Pulsante::released() {

   curState=digitalRead(pinPulsante);

   if (pulPressed) {
       if ((millis() - tPressed) > debounceDelay) {
           if (curState==LOW && preState==HIGH && !pulReleased)  {
              pulReleased=true;
              preState=LOW;
              tReleased=millis();
            }
            else
            if (pulReleased) {
               if ((millis() - tReleased) > 200) {
                 pulReleased=false;                    
                 pulPressed=false;
                 curState=LOW;
                 preState=LOW;

                 if ((tReleased - tPressed) > secPress)   {     
                    tReleased=0;
                    tPressed=0;
                    return 2;
                    }
                else { 
                    tReleased=0;
                    tPressed=0;
                    return 1;         
                }
               }
        
           }    
       }
   }        
   else 
   if  (curState==HIGH && preState==LOW && !pulPressed) {
        pulPressed=true;
        preState=HIGH;
        tPressed=millis();            
    }
   
   return 0;             

};
