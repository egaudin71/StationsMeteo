#include <U8g2lib.h>
#include <SdFat.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include <Wire.h>
#include "RTClib.h"

//---------------- RTC MODULE
DS1307 rtc;
DateTime date_now;
DateTime date_before;
//----------------Carte SD
SdFat sd;
#define PINSD 10  //pin de l'Arduino reliee au CS du module SD
#define PINCARD 7  //pin de l'Arduino reliee au CS du module SD

//---------------- DALLAS SENSOR
// Data wire is plugged into port 2 on the Arduino
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
// Pass our oneWire reference to Dallas Temperature.

#define ONE_WIRE_BUS 2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

//---------------- OLED SCREEN
U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, SCL, SDA, U8X8_PIN_NONE);   // All Boards without Reset of the Display
#define BUTTON_NEXT 3
#define BUTTON_SELECT 4
#define HEIGHT     64
#define WIDTH      128
#define debounce  250

#define bitmap_width 8
#define bitmap_height 12

static unsigned char sd_card_bits[] = {
  0x3f, 0x55, 0x81, 0x81, 0xbd, 0xa5, 0xa5, 0xa5, 0xbd, 0x81, 0x81, 0xff
};

static unsigned char sd_card_warning_bits[] = {
  0x3f, 0x55, 0x81, 0x99, 0x99, 0x99, 0x99, 0x99, 0x81, 0x99, 0x81, 0xff
};

static unsigned char file_enreg_bits[] = {
  0x18, 0x18, 0x18, 0x7e, 0x3c, 0x18, 0x7e, 0x00, 0x7e, 0x00, 0x7e, 0x00
};
static unsigned char degre_celsius_bits[] = {
  0x00, 0x00, 0x0e, 0x0a, 0xee, 0x20, 0x20, 0x20, 0xe0, 0x00, 0x00, 0x00
};
static unsigned char thermo_bits[] = {
  0x18, 0x24, 0x34, 0x24, 0x34, 0x24, 0x34, 0x24, 0x3c, 0x7e, 0x7e, 0x3c
};


char unit_time[] = "23:59:59";
char unit_date[] = "23/08/2020";

byte nbsensors = 0; // nb de capteurs detectes
DeviceAddress AddressSensors[1];
byte index = 0;
long nbEnreg = 0;
char FileSD[] = "REC01.txt";

const int micro = A0;

float tempLM35 = 0;
const char *string_list_var_0 =
  "LM35 \n"
  "Synth \n"
  "Sleep \n"
  "File";
const char *string_list_var_1 =
  "Temp 1\n"
  "LM35 \n"
  "Synth \n"
  "Sleep \n"
  "File";

//----------------------- graphique et page
volatile byte page = 0;
volatile bool cartepresente = 0;
uint8_t current_selection = 2;

//================================================
void change_page()
//================================================
{
  //----------------------- bouton et rebond
  static unsigned int last_button_time = 0;

  unsigned int button_time = millis();
  if (button_time - last_button_time > debounce)
  {
    last_button_time = button_time;
    page = 1;
  }
}


//================================================
void draw(float value, byte index = 0)
//================================================
{
  u8g2.setFont(u8g2_font_fub17_tn);
  u8g2.setFontPosTop();

  byte h_txt = (HEIGHT - u8g2.getAscent()) >> 1;//division par 2
  byte w_txt = (WIDTH - u8g2.getStrWidth(printfloat2char(value))) >> 1;//division par 2

  u8g2.drawStr( w_txt, h_txt, printfloat2char(value));

  u8g2.setFont(u8g2_font_6x12_tr);
  char txt[] = " 9";
  txt[1] = 48 + index;
//char txt='9';
  //txt = 48 + 0;
 u8g2.drawStr(0, 0, txt);
 u8g2.drawXBM( 16, 0, bitmap_width, bitmap_height, thermo_bits);
  u8g2.drawStr(0, 50, unit_time);
  u8g2.drawStr(WIDTH - u8g2.getStrWidth(unit_date), 50, unit_date);
}

//================================================
void drawClock()
//================================================
{
  u8g2.setFont(u8g2_font_fub17_tn);
  u8g2.setFontPosTop();

  byte h1 = u8g2.getAscent();
  byte h_txt = (HEIGHT - h1) >> 1;//division par 2
  byte w_txt = (WIDTH - u8g2.getStrWidth(unit_time)) >> 1;//division par 2

  u8g2.drawStr( w_txt, h_txt, unit_time);

  u8g2.setFont(u8g2_font_6x12_tr);
  w_txt = (WIDTH - u8g2.getStrWidth(unit_date)) >> 1;//division par 2
  u8g2.drawStr( w_txt, h_txt + 18, unit_date);
}

//================================================
void gettempLM35()
//================================================
{
  tempLM35 = (float)(analogRead(micro)) / 1023 * 3300;// 3.3 V pour arduino pro 3.3V
  tempLM35 /= 10.0; // conversion mill en °C
}

//================================================
char *printfloat2char( float value )
//================================================
{
  static char txt[] = "  0.9";// ou  "100.9" ou "-20.9";
  float value_l = abs(value);

  byte vv = (byte)(value_l); //sans l'arrondi
  if (value < 0)  {
    txt[0] = 45; // pour le signe moins
  }
  else {
    txt[0] = ( (vv >= 100) ? (vv / 100 + 48) : 32); // 32 pour le car espace ou les centaines
  }

  txt[1] = ( (vv >= 10) ? ((vv % 100) / 10 + 48) : 32); // les dizaines
  txt[2] = vv % 10 + 48; // les unites
  // le txt[3] est le point
  txt[4] = (byte)(10 * (value_l - 100 * (byte)(value_l / 100) - (byte)(value_l) % 100)) + 48; // la decimale pour les nb infeieurs a 100
  return txt;

}

//================================================
char *printint2char( int value )
//================================================
{
  static char txt[] = "999";// ou  "100.9" ou "-20.9";

  byte vv = (byte)(value); //sans l'arrondi
  txt[0] = ( (vv >= 100) ? (vv / 100 + 48) : 32); // 32 pour le car espace ou les centaines
  txt[1] = ( (vv >= 10) ? ((vv % 100) / 10 + 48) : 32); // les dizaines
  txt[2] = vv % 10 + 48; // les unites
  return txt;

}
//================================================
void writeFile(String txt)
//================================================
{
  SdFile fichier;

  if (!fichier.open(&sd, FileSD, O_RDWR | O_CREAT | O_AT_END)) {
    return;
  }
  fichier.print(nbEnreg);
  fichier.print(",");
  fichier.println(txt);

  fichier.sync();
  fichier.close();
  nbEnreg++;
}

//========================== clocktime
void clocktick( byte &jours, byte &heures, byte &minutes, byte &secondes)
//=======================================
{
  secondes += 1;
  if ( secondes > 59 )// sinon rien on sort
  {
    minutes += 1;
    secondes = 0;
    if ( minutes > 59 )
    {
      heures += 1;
      minutes = 0;
      if ( heures > 23 )
      {
        jours += 1;
        heures = 0;
      }
    }
  }

}

//================================================
void setup(void)
//================================================
{
  //----------------------------------------- interruption
  attachInterrupt(1, change_page, HIGH);
  pinMode(3, INPUT_PULLUP);
  pinMode(PINCARD, INPUT_PULLUP);
  //Serial.begin(9600);



  //----------------------------------------- detection du nombre de capteurs
  sensors.begin();
  nbsensors = sensors.getDeviceCount();
  for (byte i = 0; i < nbsensors; i++) {
    sensors.getAddress(AddressSensors[i], i);
  }

  //----------------------------------------- boutons du graphique
  // u8g2.setI2CAddress(0x78);
  u8g2.begin(/*Select=*/ BUTTON_SELECT, /*Right/Next=*/ BUTTON_NEXT, /*Left/Prev=*/ U8X8_PIN_NONE, /*Up=*/ U8X8_PIN_NONE, /*Down=*/ U8X8_PIN_NONE, /*Home/Cancel=*/ U8X8_PIN_NONE);
  u8g2.setFontRefHeightExtendedText();
  u8g2.setDrawColor(1);
  u8g2.setFontPosTop();
  u8g2.setFontDirection(0);

  Wire.begin();

  rtc.begin();

  if (! rtc.isrunning()) {
    //Serial.println("RTC is NOT running!");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(__DATE__, __TIME__));
  }
  //Wire.end();
  delay (1000);

  //----------------------------------------- detection de la carte SD
  // ajouter fct pour changer le nomero du fichier si existe deja un fichier

  if (!sd.begin(PINSD)) {
    //Serial.println("initialization failed!");
    cartepresente = 0;
    return;
  }
  cartepresente = 1;
  //  char FileSD[] = "REC01.txt";
  byte index = 1;

  while (sd.exists(FileSD)) {
    FileSD[3] = (index > 9 ? index / 10 + 48 : 48); //48 = Zero
    FileSD[4] = index % 10 + 48;
    index++;
  }
  //now = rtc.now();
  date_before = rtc.now();
  //  date_min = now.minute();
  //  lastminute = date_min;
  //Serial.println("initialization done.");

}

/*
   Main function, get and show the temperature
*/
//================================================
void loop(void)
//================================================
{
  //unsigned long currenttime = millis();
  float temp[1];
  int8_t bouton = u8g2.getMenuEvent();
  byte cardOK = digitalRead(PINCARD);


  if (cardOK == LOW) {
    cartepresente = 0;// la carte a ete retiree pas d'ecriture
  }

  //DateTime
  date_now = rtc.now(); //recuperation date du RTC

  gettempLM35();

  sensors.requestTemperatures(); // Send the command to get temperatures
  for (byte i = 0; i < nbsensors; i++) {
    temp[i] = sensors.getTempC(AddressSensors[i]);
  }

  if ( date_now.unixtime() > date_before.unixtime()) { // toutes les 2 secondes
    date_before = date_now;
    sprintf(unit_time, "%02d:%02d:%02d", date_now.hour(), date_now.minute(), date_now.second());
    sprintf(unit_date, "%02d/%02d/%04d", date_now.day(), date_now.month(), date_now.year());


    String txtsd = unit_date;
    txtsd = txtsd + "," + unit_time ;
    txtsd = txtsd + "," + printfloat2char(temp[0]) ;
    txtsd = txtsd + "," + printfloat2char(tempLM35) ;


    if ( (date_now.minute() != date_before.minute()) && (date_now.minute() % 2 == 0)) { // toutes les 2 minutes

      if ((cartepresente == 0) && (cardOK == HIGH)) { // la carte avait ete enlevee, elle est remise
        cartepresente = 1;// reactive la communication
        if (!sd.begin(PINSD)) {
          //Serial.println("initialization failed!");
          return;
        }
      }

      writeFile(txtsd);
    }

  }


  if (page == 1) {
    //-------------------------- Page des MENUS
    u8g2.setFont(u8g2_font_6x12_tr);
    switch (nbsensors) {
      case 0:
        current_selection = u8g2.userInterfaceSelectionList("Mesures", current_selection, string_list_var_0);
        break;
      case 1:
        current_selection = u8g2.userInterfaceSelectionList("Mesures", current_selection, string_list_var_1);
        break;
      default:
        break;
    }
    page = 0;
  }
  else {
    // picture loop
    u8g2.firstPage();
    do {

      if (current_selection <= nbsensors) {
        //--------------------------------- page de chaque CAPTEUR
        draw(temp[current_selection - 1 ], current_selection);
      }
      else if (current_selection == nbsensors + 1) {
        draw(tempLM35, current_selection);
      }
      else if (current_selection == nbsensors + 2) {
        drawClock();
        u8g2.setFont(u8g2_font_6x12_tr);
        u8g2.setFontPosTop();
        u8g2.drawStr( 0, 0, printfloat2char(temp[0]));
        u8g2.drawXBM( u8g2.getStrWidth(printfloat2char(temp[0])), 0, bitmap_width, bitmap_height, degre_celsius_bits);
        u8g2.drawStr( WIDTH - u8g2.getStrWidth(printfloat2char(tempLM35)) - 8, 0, printfloat2char(tempLM35));
        u8g2.drawXBM( WIDTH - 8 , 0, bitmap_width, bitmap_height, degre_celsius_bits);

        char txt[] = "# 10000";
        sprintf(txt, "#%d", nbEnreg);
        byte l1 = u8g2.getStrWidth(txt);

        u8g2.drawStr(WIDTH - l1, 54, txt);

        byte cardOK = digitalRead(PINCARD);
        if (cardOK == LOW) {
          u8g2.drawXBM(0, HEIGHT - bitmap_height, bitmap_width, bitmap_height, sd_card_warning_bits );
        } else {
          u8g2.drawXBM(0, HEIGHT - bitmap_height, bitmap_width, bitmap_height, sd_card_bits);
          u8g2.drawXBM(12, HEIGHT - bitmap_height, bitmap_width, bitmap_height, file_enreg_bits);
          u8g2.drawStr(12 + bitmap_width + 8 , 54, FileSD);
        }

      }
      else if (current_selection == nbsensors + 3) {
        u8g2.drawStr(0, 0, "");
      }
      else {
        u8g2.setFont(u8g2_font_6x12_tr);
        u8g2.setFontPosTop();
        u8g2.drawStr( 0, 0, "FICHIER");
        byte cardOK = digitalRead(PINCARD);
        u8g2.drawStr( 0, 15, (cardOK == LOW) ? "No CARD !" : "Card OK");
        if ( cardOK == HIGH ) {
          u8g2.drawStr( 0, 30, FileSD);
          SdFile fichier;
          if (!fichier.open(&sd, FileSD, O_READ))
          {
            return;
          }
          uint32_t taille = fichier.fileSize() / 1024;
          char txt[] = "Taille 1000 ko";
          sprintf(txt, "Taille %d ko", taille);
          u8g2.drawStr( 0, 40, txt);
          fichier.sync();
          fichier.close();
        }
      }

    } while ( u8g2.nextPage() );
  }

}
