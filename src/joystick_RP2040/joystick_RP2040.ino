/*
Infineon謹製ライブラリ(https://github.com/Infineon/TLE493D-3DMagnetic-Sensor)
でA0以外のセンサ使った場合の挙動がおかしいので、センサとのI2C通信部分はArduino標準ライブラリのみで記述しています。
詳細はイチケンのブログ記事を参照してください。
https://ichiken-engineering.com/3d_hallsensor_gamepad/
参考にした記事など
https://fabacademy.org/2022/labs/charlotte/students/alaric-pan/assignments/week13/
https://academy.cba.mit.edu/classes/input_devices/mag/TLE493D/hello.TLE493D.t412.ino
*/
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_TinyUSB.h>//書き込み時は[Tools]>[USB Stack]>[Tiny USB]
#include <Adafruit_GFX.h>
#include "joystick.h"

#define DEBUG 0

#define BTN_X 20
#define BTN_Y 21
#define BTN_A 18
#define BTN_B 19

#define BTN_UP 10
#define BTN_DOWN 13
#define BTN_RIGHT 11
#define BTN_LEFT 12

#define BTN_L1 3
#define BTN_L2 2
#define BTN_R1 0
#define BTN_R2 1

#define BTN_START 17
#define BTN_SELECT 16

#define LED_CTRL 6
#define SELECT_DEMOMODE 9

//画面のサイズの設定
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

//左右のジョイスティックそれぞれに使われているセンサの指定
//配布版は左A1,右A2のはず
Tle493d JoyL(TLE493D_A1);
Tle493d JoyR(TLE493D_A2);

//ジョイスティックを目一杯倒したときの上限下限の絶対値
const int joyRange = 300;

//ジョイスティック押し込み操作の閾値
const int joyPushStroke = 120;

//ジョイスティックのアソビ
//この1/2がデッドゾーンになる
const int asobi = 180;

//明滅タイマ関連
repeating_timer_t timer; 
uint8_t timerBitShift = 1;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);

uint8_t const desc_hid_report[] = {
  TUD_HID_REPORT_DESC_GAMEPAD()
};
Adafruit_USBD_HID usb_hid(desc_hid_report, sizeof(desc_hid_report), HID_ITF_PROTOCOL_NONE, 2, false);
hid_gamepad_report_t gp;


uint8_t keyLog[20] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
uint8_t knm[]= {0,6,0,5,0,4,0,3,0,4,0,3,0,2,0,2,0,1,0,1};//謎の配列
const uint8_t logNum = 20;
bool secretFlag = false;//謎のフラグ

//タイマ割込みで呼ばれる関数
bool timer_callback(repeating_timer_t*){
  //時刻から正弦波を作ってLEDを明滅
  float t = ((millis()>>timerBitShift)%360)*PI/180.0;
  analogWrite(LED_CTRL,  (int)((sin(t)+1)*127));

  return true;
}

//ボタンの状態を表す変数btnsの指定のbitにボタンの状態を書き込む。
void updateBtn(int *btns, int position, byte value) {
  value == 0 ? *btns &= ~(1 << position) : *btns |= (1 << position);
}


//指定のピン番号のボタンの状態を確認して、構造体gpに書き込み。
bool checkBtn(hid_gamepad_report_t *gp, uint8_t port, uint8_t position){
  if (digitalRead(port) == LOW) {
    gp->buttons |= (1 << position);
    return true;
  } else {
    gp->buttons &= ~(1 << position);
    return false;
  }
}

//ハットスイッチの状態確認。同時押し斜め入力の判定など。
uint8_t checkHat(uint8_t btns){
  uint8_t temp = btns;
  uint8_t hat=0;
  for (int i=0;i<4;i++){
    if(btns%2){
      hat = i*2+1;
      break;
    }
    btns = btns>>1 | btns<<3;
  }
  if((btns>>1)%2){
    hat++;
  }else if((btns>>3)%2){
    hat = 8;
  }
  return hat;
}

//配列を比較して、それが一致しているか確認する。
bool areArraysEqual(uint8_t *arr1, uint8_t *arr2, int n) {
    for (int i = 0; i < n; i++) {
        if (arr1[i] != arr2[i]) {
            // 配列の要素が一致しない場合
            return false;
        }
    }

    // 配列のすべての要素が一致する場合
    return true;
}

/*押されているキーのログを記録する配列に、新規の値を書き込む。
既存の値は後方にシフトし、古いものから破棄。*/
void keyLogShift(uint8_t *kl, uint8_t keyStatus){
  if (kl[0] == keyStatus){
    return;
  }
  for (int i = logNum - 1; i > 0; i--){
    kl[i] = kl[i-1];
  }
  kl[0] = keyStatus;
  return;
}

void setup() {
  /*-----------------------------初期化処理-----------------------------*/
  //起動ログ表示の持続時間
  int dispStepTime = 200;

  pinMode(BTN_X, INPUT_PULLUP);
  pinMode(BTN_Y, INPUT_PULLUP);
  pinMode(BTN_A, INPUT_PULLUP);
  pinMode(BTN_B, INPUT_PULLUP);

  pinMode(BTN_L1, INPUT_PULLUP);
  pinMode(BTN_L2, INPUT_PULLUP);
  pinMode(BTN_R1, INPUT_PULLUP);
  pinMode(BTN_R2, INPUT_PULLUP);

  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);

  
  pinMode(BTN_START, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);

  pinMode(LED_CTRL, OUTPUT);
  pinMode(SELECT_DEMOMODE, INPUT_PULLUP);

  //タイマ割込み
  add_repeating_timer_ms(10, &timer_callback, NULL, &timer);

  /*アドレス指定してディスプレイ起動。
  電源繋いでも何も表示されない場合、ディスプレイの起動に失敗している(はんだ付け不良による接触不良が多い)か、あるいはHIDとしての通信確立に失敗している。*/
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    //Serial.println(F("SSD1306 can not allocate memory!"));
    return;
  }

  /*HIDとしてPCと通信確立する。
  Windows PC以外での動作は未確認*/
  TinyUSB_Device_Init(0);
  usb_hid.begin();

  /*-----------------------------どこかで見たことのあるオープニング演出-----------------------------*/
  display.clearDisplay();
  display.fillRect(0, 0, 8, sizeof(ichiken)/(sizeof(char)*20)*8, WHITE);
  for(int i=0; i<sizeof(ichiken)/(sizeof(char)*20); i++){
    display.setTextColor(WHITE);
    display.setCursor(2, i*8);
    display.println(ichiken[i]);
    display.setTextColor(BLACK, WHITE);
    display.setCursor(1, i*8);
    display.println(ichiken[i][0]);
  }
  display.display();
  delay(1000);
  display.setTextColor(WHITE, BLACK);
  for(int i=0; i<sizeof(ichiken)/(sizeof(char)*20); i++){
    display.fillRect(0, i*8, SCREEN_WIDTH, 8, BLACK);
    display.setCursor(1, i*8);
    display.print(ichiken[i][0]);
    for(int j=1; j<20; j++){
      display.setCursor(j*6+2, i*8);
      display.print((ichiken[i][j] >= 'a' && ichiken[i][j] <= 'z') ? (char)(ichiken[i][j] - 'a' + 'A') : ichiken[i][j]);
    }
    display.display();
    delay(dispStepTime);
  }
  delay(dispStepTime);

  display.setTextColor(BLACK);
  display.clearDisplay();
  display.drawBitmap(0,0,secret, SCREEN_WIDTH, SCREEN_HEIGHT, 1);
  display.setCursor(25, 40);
  display.println("GAMEPAD");
  if(!digitalRead(SELECT_DEMOMODE)){
    display.setCursor(0, 56);
    display.println("DEMO MODE");
  }
  display.display();
  delay(2000);

  /*-----------------------------起動チェックシーケンス-----------------------------*/
  display.setTextSize(1);
  display.setCursor(0, 0);

  display.clearDisplay();
  display.drawBitmap(0,0,secret, SCREEN_WIDTH, SCREEN_HEIGHT, 1);
  // display.fillRect(左上x, 左上y, 幅, 高さ, 線の色)
  display.fillRect(0, 0, 70, 64, WHITE);

  display.print("Serial:");
  display.display();
  delay(dispStepTime);
  #if DEBUG
    Serial.begin(9600);
    while (!Serial);
    display.println("OK");
  #else
    display.println("SKIP");
  #endif

  display.display();
  delay(dispStepTime);

  Wire.begin();
  Wire.setClock(400000);
  /*Wireのセットアップメソッドに戻り値が無いので、成功/失敗の判定ができない。
  ほぼ確実にOK表示になる。*/
  display.println("I2C:OK");
  display.display();
  delay(dispStepTime);

  display.print("TinyUSB:");
  display.display();
  /*PC以外のものに接続された場合、ここで止まることが多い。
  デモモードの場合はスキップ(Ver1.1以降)*/
  if(digitalRead(SELECT_DEMOMODE)){
    while (!TinyUSBDevice.mounted());
    display.println("OK");
  }else{
    display.println("SKIP");
  }
  display.display();

  display.print("HID:");
  display.display();
  if(digitalRead(SELECT_DEMOMODE)){
    while (!usb_hid.ready());
    display.println("OK");
  }else{
    display.println("SKIP");
  }
  display.display();
  delay(dispStepTime);

  /*右センサ通信開始。接続ヨイカ？*/
  display.print("Joy R:");
  display.display();
  if(JoyR.begin()){
    JoyR.asobi = asobi;
    display.println("OK");
    display.display();
  }else{
    display.println("ERROR");
    display.display();
    while(1);
  }

  /*左センサ通信開始。接続ヨイカ？*/
  display.print("Joy L:");
  display.display();
  if(JoyL.begin()){
    JoyL.asobi = asobi;
    display.println("OK");
    display.display();
  }else{
    display.println("ERROR");
    display.display();
    while(1);
  }
  delay(dispStepTime);

  /*-----------------------------キャリブレーション-----------------------------*/
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextColor(WHITE);

  /*左右センサのキャリブレーション。
  触れられていない状態の数値を記録したり、磁石の極の向きを記録したりする*/
  display.setTextSize(2);
  display.println("HANDS OFF");
  display.setTextSize(1);
  display.println("Calibration\n    in Progress");
  display.display();
  delay(1000);

  JoyR.calibrate();
  display.println("R:(" + String(JoyR.xRaw) + ", " + String(JoyR.yRaw) + ", " + String(JoyR.zRaw) + ")");
  display.display();

  JoyL.calibrate();
  display.println("L:(" + String(JoyL.xRaw) + ", " + String(JoyL.yRaw) + ", " + String(JoyL.zRaw) + ")");
  display.display();
  cancel_repeating_timer (&timer);
  analogWrite(LED_CTRL, 0);
  delay(dispStepTime);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.println("All\nSystems\nReady!!");
  display.display();

  /*-----------------------------起動完了。定常状態に移行するための初期化処理-----------------------------*/
  for(int i=0;i<256;i++){
    analogWrite(LED_CTRL, i);
    delay(4);
  }

  gp.x = 0;
  gp.y = 0;
  gp.z = 0;
  gp.rz = 0;
  gp.rx = 0;
  gp.ry = 0;
  gp.hat = 0;
  gp.buttons = 0;
  usb_hid.sendReport(0, &gp, sizeof(gp));

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);  //文字色
  display.setTextSize(1);

  timerBitShift = 3;

}


void loop() {
  int rxd,ryd,rzd,lxd,lyd,lzd;
  int btnFlag = 0b00000000;
  int hatFlag = 0b00000000;
  static int mashFlag = 0b0000;

  const int btnMask = 0b1111111111;
  const int xyab_x = 72;//xyabのインジケータを表示するX座標
  const int hatIndicaterWidth = 3;
  

  //左右センサ状態更新
  JoyR.update();
  JoyL.update();
  
  display.clearDisplay();
  display.setTextColor(WHITE);

  /*-----------------------------XYAB-----------------------------*/
  if (checkBtn(&gp, BTN_X, 0)) {
    display.setCursor(xyab_x, 0);
    if(mashFlag & 0b0001){
      display.setTextColor(BLACK, WHITE);
    }
    display.print("X");
    display.setTextColor(WHITE);
  }

  if (checkBtn(&gp, BTN_Y, 3)) {
    display.setCursor(xyab_x + 8, 0);
    if(mashFlag & 0b1000){
      display.setTextColor(BLACK, WHITE);
    }
    display.print("Y");
    display.setTextColor(WHITE);
  }

  if (checkBtn(&gp, BTN_A, 1)) {
    display.setCursor(xyab_x + 8*2, 0);
    if(mashFlag & 0b0010){
      display.setTextColor(BLACK, WHITE);
    }
    display.print("A");
    display.setTextColor(WHITE);
    keyLogShift(keyLog,6);
  }

  if (checkBtn(&gp, BTN_B, 2)) {
    display.setCursor(xyab_x + 8*3, 0);
    if(mashFlag & 0b0100){
      display.setTextColor(BLACK, WHITE);
    }
    display.print("B");
    display.setTextColor(WHITE);
    keyLogShift(keyLog,5);
  }

  /*-----------------------------LR-----------------------------*/
  if (checkBtn(&gp, BTN_L1, 4)) {
    display.setCursor(0, 0);
    display.print("L1");
  }

  if (checkBtn(&gp, BTN_L2, 6)) {
    display.setCursor(8*2, 0);
    display.print("L2");
  }

  if (checkBtn(&gp, BTN_R1, 5)) {
    display.setCursor(8*4, 0);
    display.print("R1");
  }

  if (checkBtn(&gp, BTN_R2, 7)) {
    display.setCursor(8*6, 0);
    display.print("R2");
  }

  /*-----------------------------ジョイスティック押し込み-----------------------------*/
  //XY入力範囲端にいくほどZ押し込みの閾値が下がる二次関数
  if (JoyR.z > -1.0/1100*pow(JoyR.r,2)+joyPushStroke) {
    updateBtn(&btnFlag, 11, 1);
  } else {
    updateBtn(&btnFlag, 11, 0);
  }

  if (JoyL.z > -1.0/1100*pow(JoyL.r,2)+joyPushStroke) {
    updateBtn(&btnFlag, 10, 1);
  } else {
    updateBtn(&btnFlag, 10, 0);
  }

  /*-----------------------------Hat-----------------------------*/
  if (digitalRead(BTN_UP) == LOW) {
    updateBtn(&hatFlag, 0, 1);
    keyLogShift(keyLog,1);
  } else {
    updateBtn(&hatFlag, 0, 0);
  }

  if (digitalRead(BTN_RIGHT) == LOW) {
    updateBtn(&hatFlag, 1, 1);
    keyLogShift(keyLog,4);
  } else {
    updateBtn(&hatFlag, 1, 0);
  }
  
  if (digitalRead(BTN_DOWN) == LOW) {
    updateBtn(&hatFlag, 2, 1);
    keyLogShift(keyLog,2);
  } else {
    updateBtn(&hatFlag, 2, 0);
  }

  if (digitalRead(BTN_LEFT) == LOW) {
    updateBtn(&hatFlag, 3, 1);
    keyLogShift(keyLog,3);
  } else {
    updateBtn(&hatFlag, 3, 0);
  }

  /*-----------------------------Start Select-----------------------------*/
  if (checkBtn(&gp, BTN_START, 9)) {
    display.setCursor(58, 56);
    display.print("ST");
  }

  if (checkBtn(&gp, BTN_SELECT, 8)) {
    display.setCursor(58, 48);
    display.print("SL");
  }

  //Start,Slect同時押しされている場合
  if ((gp.buttons>>8 & 0b11) == 0b11) {
    display.setCursor(58, 40);
    display.print("MS");
    //自動連打フラグにその時押しているボタンを追加
    mashFlag = gp.buttons & 0b1111;
  }
  
  //キーログチェック
  if(areArraysEqual(keyLog, knm, logNum)){
    keyLogShift(keyLog,99);
    if(secretFlag){
      cancel_repeating_timer (&timer);
      digitalWrite(LED_CTRL,HIGH);
    }else{
      add_repeating_timer_ms(50, &timer_callback, NULL, &timer);
    }
    secretFlag = !secretFlag;
  }
  
  gp.buttons = (gp.buttons & btnMask) | btnFlag;

  gp.hat = checkHat(hatFlag);

  //ボタンが何も押されていなければ、無入力をキーログに記録
  if(hatFlag == 0 && gp.buttons == 0){
    keyLogShift(keyLog,0);
  }

  /*自動連打フラグが有効のボタンは、2^5=32msおきに入力/無効を切り替える*/
  if((millis()>>5)%2){
    gp.buttons = gp.buttons & (0b1111110000 | ~mashFlag);
  }

  gp.x = (int8_t)(map(constrain(JoyL.x, -joyRange, joyRange), -joyRange, joyRange, -127, 127));
  gp.y = (int8_t)(map(constrain(JoyL.y, -joyRange, joyRange), -joyRange, joyRange, -127, 127));

  gp.z = (int8_t)(map(constrain(JoyR.x, -joyRange, joyRange), -joyRange, joyRange, -127, 127));
  gp.rz = (int8_t)(map(constrain(JoyR.y, -joyRange, joyRange), -joyRange, joyRange, -127, 127));



  usb_hid.sendReport(0, &gp, sizeof(gp));

  rxd = map(constrain(-JoyR.x, -joyRange, joyRange), -joyRange, joyRange, 127, 72);
  ryd = map(constrain(-JoyR.y, -joyRange, joyRange), -joyRange, joyRange, 63, 8);
  rzd = map(constrain(JoyR.z, -600, 200), -600, 200, 2, 10);

  lxd = map(constrain(-JoyL.x, -joyRange, joyRange), -joyRange, joyRange, 55, 0);
  lyd = map(constrain(-JoyL.y, -joyRange, joyRange), -joyRange, joyRange, 63, 8);
  lzd = map(constrain(JoyL.z, -600, 200), -600, 200, 2, 10);

  // display.fillRect(左上x, 左上y, 幅, 高さ, 線の色)
  display.fillRect(0, 8, 55, 55, WHITE);
  display.fillRect(72, 8, 55, 55, WHITE);
  
  //ハット入力マーカ表示
  switch (gp.hat){
    case 1:
      display.fillRect(18, 8, 18, 18, BLACK);
      break;

    case 2:
      display.fillRect(37, 8, 18, 18, BLACK);
      break;
    
    case 3:
      display.fillRect(37, 27, 18, 18, BLACK);
      break;

    case 4:
      display.fillRect(37, 46, 18, 18, BLACK);
      break;
      
    case 5:
      display.fillRect(18, 46, 18, 18, BLACK);
      break;

    case 6:
      display.fillRect(0, 46, 18, 18, BLACK);
      break;
          
    case 7:
      display.fillRect(0, 27, 18, 18, BLACK);
      break;

    case 8:
      display.fillRect(0, 8, 18, 18, BLACK);
      break;

    default:
      break;
  }
  
  display.fillRect(hatIndicaterWidth, 8+hatIndicaterWidth, 55-hatIndicaterWidth*2, 55-hatIndicaterWidth*2, WHITE);

  if((gp.buttons>>11)%2){
    display.fillCircle(rxd, ryd, rzd, BLACK);
  }else{
    display.drawCircle(rxd, ryd, rzd, BLACK);
  }
  if((gp.buttons>>10)%2){
    display.fillCircle(lxd, lyd, lzd, BLACK);
  }else{  
    display.drawCircle(lxd, lyd, lzd, BLACK);
  }

  display.setTextColor(BLACK);
  display.setCursor(72, 40);
  display.print("X:"+String(JoyR.x));
  display.setCursor(72, 48);
  display.print("Y:"+String(JoyR.y));
  display.setCursor(72, 56);
  display.print("Z:"+String(JoyR.z));

  display.setCursor(0, 40);
  display.print("X:"+String(JoyL.x));
  display.setCursor(0, 48);
  display.print("Y:"+String(JoyL.y));
  display.setCursor(0, 56);
  display.print("Z:"+String(JoyL.z));

  if (secretFlag){
    int bmpXShift = map(JoyL.x, -joyRange, joyRange, -10, 10) ;
    int bmpYShift = map(JoyL.y, -joyRange, joyRange, -10, 10) ;
    timerBitShift = map(constrain(JoyR.y, -joyRange, joyRange), -joyRange, joyRange, 0, 4) ;
    display.clearDisplay();
    display.drawBitmap(0+bmpXShift,0+bmpYShift,secret, SCREEN_WIDTH, SCREEN_HEIGHT, 1);
  }else{
    digitalWrite(LED_CTRL,HIGH);
  }

  display.display();
  delay(8);//だいたい120Hzくらい
}