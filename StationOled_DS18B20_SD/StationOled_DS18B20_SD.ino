#include <U8g2lib.h>
#include <SdFat.h>
#include <OneWire.h>
#include <DallasTemperature.h>

SdFat sd;
#define PINSD 10  //pin de l'Arduino reliee au CS du module SD
#define PINCARD 7  //pin de l'Arduino reliee au CS du module SD

// Data wire is plugged into port 2 on the Arduino
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
// Pass our oneWire reference to Dallas Temperature.
#define ONE_WIRE_BUS 2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

U8G2_SSD1306_128X64_NONAME_1_SW_I2C u8g2(U8G2_R0, SCL, SDA, U8X8_PIN_NONE);   // All Boards without Reset of the Display
#define HEIGHT     64
#define WIDTH      128
#define debounce  250

byte date_sec = 0;
byte date_min = 0;
byte date_hrs = 0;
byte date_jrs = 0;
long lasttime = 0;
byte lastminute;

char unit_time[] = "23:59:59";

byte nbsensors = 0; // nb de capteurs detectes
DeviceAddress AddressSensors[3];
byte index = 0;
long nbEnreg = 0;
char FileSD[] = "REC01.txt";

int micro = A0;
float sound = 0;
float sound2 = 0;

const char *string_list_var =
  "Temp 1\n"
  "Temp 2\n"
  "Micro \n"
  "Synth \n"
  "File";

//----------------------- graphique et page
volatile byte page = 0;
volatile bool cartepresente = 0;
uint8_t current_selection = 4;

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
  char txt[] = "CAPTEUR 9";
  txt[8] = 48 + index;
  u8g2.drawStr(0, 0, txt);
  u8g2.drawStr(0, 50, unit_time);
}

//================================================
void getsound()
//================================================
{
   int a = 0;
  
  a = analogRead(micro);
  
  sound = a; sound2 = a*a;
 a = analogRead(micro);
  sound += a; sound2 += a*a;
 a = analogRead(micro);
  sound += a; sound2 += a*a;
 a = analogRead(micro);
  sound += a; sound2 += a*a;
  sound /=4; // moyennedivision par 4
   sound2 /=4;// moyennedivision par 4
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
  u8g2.begin(/*Select=*/ 4, /*Right/Next=*/ 3, /*Left/Prev=*/ U8X8_PIN_NONE, /*Up=*/ U8X8_PIN_NONE, /*Down=*/ U8X8_PIN_NONE, /*Home/Cancel=*/ U8X8_PIN_NONE);
  u8g2.setFontRefHeightExtendedText();
  u8g2.setDrawColor(1);
  u8g2.setFontPosTop();
  u8g2.setFontDirection(0);

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
date_min = 0;
  //Serial.println("initialization done.");
}

/*
   Main function, get and show the temperature
*/
//================================================
void loop(void)
//================================================
{
  unsigned long currenttime = millis();
  float temp[3];
  int8_t bouton = u8g2.getMenuEvent();
  byte cardOK = digitalRead(PINCARD);

  if (cardOK == LOW) {
    cartepresente = 0;// la carte a ete retiree pas d'ecriture
  }

  if ( (unsigned long)(currenttime - lasttime) >= 1000 ) {// toutes les 2 secondes
    lasttime = currenttime;
    clocktick( date_jrs, date_hrs, date_min, date_sec);
    sprintf(unit_time, "%02d:%02d:%02d", date_hrs, date_min, date_sec);

    getsound();// microphone

    sensors.requestTemperatures(); // Send the command to get temperatures
    for (byte i = 0; i < nbsensors; i++) {
      temp[i] = sensors.getTempC(AddressSensors[i]);
    }

    String txtsd = unit_time;
    txtsd = txtsd + "," + printfloat2char(temp[0]) ;
    txtsd = txtsd + "," + printfloat2char(temp[1]) ;
    txtsd = txtsd + "," + printfloat2char(sound) ;


    if ( (date_min != lastminute)&&(date_min % 2==0)) {// toutes les 1 minutes
      lastminute = date_min;

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
    current_selection = u8g2.userInterfaceSelectionList("Mesures", current_selection, string_list_var);
    page = 0;
  }
  else {
    // picture loop
    u8g2.firstPage();
    do {
      if (current_selection < 2) {
        //--------------------------------- page de chaque CAPTEUR
        draw(temp[current_selection - 1], current_selection);
      }
      else if (current_selection == 3) {
        draw(sound2, current_selection);        
      }
      else if (current_selection == 5) {
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
      else  {
        u8g2.setFont(u8g2_font_6x12_tr);
        u8g2.setFontPosTop();
        u8g2.drawStr( 0, 0, printfloat2char(temp[0]));
        u8g2.drawStr(64, 0, printfloat2char(temp[1]));
        u8g2.drawStr( 0, 12,printfloat2char(sound));
        u8g2.drawStr(0, 20, unit_time);
        char txt[] = "Enreg#######";
        sprintf(txt, "Enreg# %d", nbEnreg);
        u8g2.drawStr(0, 30, txt);

        u8g2.drawStr(0, 40, FileSD);

        byte cardOK = digitalRead(PINCARD);
        u8g2.drawStr(30, 52, (cardOK == LOW) ? "No CARD !" : "Card OK");
        if (cartepresente == 0) {
          u8g2.drawStr(50, 40, "/!\\ init");
        }
      }
    } while ( u8g2.nextPage() );
  }

}
