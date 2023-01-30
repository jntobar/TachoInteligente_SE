#include <LiquidCrystal_I2C.h>

#define lcdColumns 16
#define lcdRows  2
#define SDA 12 //azul
#define SCL 13 //morado

LiquidCrystal_I2C lcd(PCF8574_ADDR_A21_A11_A01, 4, 5, 6, 16, 11, 12, 13, 14, POSITIVE);
//LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);

void EncenderLCD(String msj1, IPAddress msj2)
{
  lcd.begin(lcdColumns, lcdRows);
  Wire.begin(SDA,SCL);
  lcd.backlight(); //Encedmos la pantalla
  lcd.setCursor(0,0);
  lcd.print(msj1);
  lcd.setCursor(0,1);
  lcd.print(msj2);
  
}
