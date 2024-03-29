//各型番ごとのI2Cアドレス
enum TypeAddress_e{
  TLE493D_A0 = 0x35,
  TLE493D_A1 = 0x22,
  TLE493D_A2 = 0x78,
  TLE493D_A3 = 0x44
};

class Tle493d{
  public:
    Tle493d(TypeAddress_e address);
    ~Tle493d();
    int x;
    int y;
    int z;
    float r;
    int xRaw;
    int yRaw;
    int zRaw;
    bool begin();
    void update();
    void calibrate();
    bool pole;
    int asobi;

  private:
    TypeAddress_e address;
    int xPrev;
    int yPrev;
    int zPrev;
    int xOffset;
    int yOffset;
    int zOffset;
    void resetSenser();
    bool configSenser();
    int asobiAdj(int axis);
};

/*コンストラクション
変数の初期化が多いのでこの方が読みやすい*/
Tle493d::Tle493d(TypeAddress_e addr){
  x=0;y=0;z=0;r=0;
  xRaw=0;yRaw=0;zRaw=0;
  pole=true;
  xPrev=0;yPrev=0;zPrev=0;
  xOffset=0;yOffset=0;zOffset=0;
  address = addr;
  asobi = 200;
}

Tle493d::~Tle493d(){
}

//必ずWire.begin();の後である必要がある。
bool Tle493d::begin(){
  bool result;
  //基本的にアドレス書き換えなどしていないけど念のためリセット
  resetSenser();
  result = configSenser();
  return result;
}

//初期状態(手が触れていない状態の値と磁石の極の向き)を記録しておく
void Tle493d::calibrate(){
  uint8_t data[6];
  Wire.requestFrom(address,6);
  for (int i = 0; i < 6; ++i) {
    data[i] = Wire.read();
  }
  if((data[2] << 4)+(data[5] & 0x0F)>2047){//磁石の裏表を判定
    pole = false;
  }else{
    pole = true;
  }
  update();
  xOffset = xRaw;
  yOffset = yRaw;
  zOffset = zRaw;
  z=0;
}

/*センサにリセットシグナルを送る。*/
void Tle493d::resetSenser(){
  Wire.beginTransmission(address);
  Wire.write(0xFF);
  Wire.endTransmission();
  Wire.beginTransmission(address);
  Wire.write(0xFF);
  Wire.endTransmission();
  Wire.beginTransmission(address);
  Wire.write(0x00);
  Wire.endTransmission();
  Wire.beginTransmission(address);
  Wire.write(0x00);
  Wire.endTransmission();
  delayMicroseconds(50);
}

/*センサのアドレス部分しか書き換えていないので、パリティビットは決め打ち
アドレスはデフォルトから変更ないけど念のため*/
bool Tle493d::configSenser(){
  byte error;
  uint8_t config = 0b00101000;
  uint8_t mod1 = 0b00010101;

  //[7]parity[6:5]IICAdr
  switch (address)
  {
    case TLE493D_A1:
      mod1 |= (0b101 << 5);
      break;
    
    case TLE493D_A2:
      mod1 |= (0b110 << 5);
      break;

    case TLE493D_A3:  
      mod1 |= (0b011 << 5);

    default:
      break;
  }

  Wire.beginTransmission(address);
  Wire.write(0x10);
  Wire.write(config);
  Wire.write(mod1);
  error = Wire.endTransmission();
  if(error == 0){
    return true;
  }else{
    return false;
  }
}

/*アソビ範囲内にある値の補正*/
int Tle493d::asobiAdj(int axis){
  int axisAdj;
  if(r < asobi/2){
    axisAdj = 0;
  }else if(r < asobi){
    axisAdj = axis * (2 - asobi/r);
  }
  return axisAdj;
}

/*状態更新*/
void Tle493d::update(){
  uint8_t data[6];
  int transformedData1[3];
  float lpf = 0;
  float adj = 0;

  Wire.requestFrom(address,6);
  for (int i = 0; i < 6; ++i) {
    data[i] = Wire.read();
  }

  transformedData1[0] = (data[0] << 4)+((data[4] & 0xF0) >> 4);
  transformedData1[1] = (data[1] << 4)+(data[4] & 0x0F);
  transformedData1[2] = (data[2] << 4)+(data[5] & 0x0F);

  for (int i = 0; i < 3; ++i) {
    if (transformedData1[i] > 2047) transformedData1[i] -= 4096;

  }
  if(!pole){//磁石の裏表によって値を反転
    xRaw = transformedData1[1];
    yRaw = transformedData1[0];
    zRaw = -transformedData1[2];
  }else{
    xRaw = -transformedData1[1];
    yRaw = -transformedData1[0];
    zRaw = transformedData1[2];
  }
    
  r = sqrt(pow(xRaw,2) + pow(yRaw,2));
  xPrev = x;
  yPrev = y;
  zPrev = z;
  if (r < (float)asobi){//アソビの範囲内なら
    lpf = 0.2 + 0.8*(r/asobi);//中心に近いほどLPFが強くかかる
    x = (int)(xPrev*(1-lpf) + asobiAdj(xRaw)*lpf);//LPFに加え、0に近いほど0に向かおうとする
    y = (int)(yPrev*(1-lpf) + asobiAdj(yRaw)*lpf);//LPFに加え、0に近いほど0に向かおうとする
  }else{
    x = xRaw;
    y = yRaw;
  }
  z = (int)(zPrev*0.7 + (zRaw-zOffset)*0.3);
}

//謎のビットマップ
const unsigned char secret[] PROGMEM = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf3, 0xfe, 0x1f, 0xff, 0xe1, 0xef, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe7, 0xff, 0xc7, 0xfc, 0x09, 0x07, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xdf, 0xff, 0xf3, 0xf8, 0x38, 0xef, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x9f, 0xff, 0xfd, 0xd2, 0x7f, 0xed, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xbf, 0xff, 0x1e, 0x07, 0xff, 0xe1, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xbf, 0xf8, 0xcf, 0x0f, 0xff, 0xc4, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xbf, 0xe7, 0xf7, 0x6f, 0xff, 0xfd, 0xbf, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xbf, 0x9f, 0xfb, 0x7f, 0xff, 0xfc, 0x3f, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x8f, 0x3f, 0xfc, 0x7f, 0xff, 0xfc, 0xbf, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x26, 0xff, 0x3c, 0xff, 0xff, 0xfd, 0xbf, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x71, 0xfc, 0xbc, 0xff, 0xff, 0xff, 0xbf, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x73, 0xf9, 0xd9, 0xff, 0xff, 0xff, 0x8f, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x3b, 0xf3, 0xcb, 0xff, 0xff, 0xff, 0x8f, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xbb, 0xe7, 0xe3, 0xff, 0xff, 0xff, 0xe7, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xb9, 0x8f, 0xf2, 0x7f, 0xff, 0xff, 0xe7, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x9d, 0xcf, 0xfb, 0x3f, 0xff, 0xff, 0xe7, 
	0xe0, 0xc0, 0x31, 0xce, 0x18, 0xe3, 0x00, 0xc7, 0x1f, 0x9d, 0xe7, 0xfb, 0x9f, 0x1f, 0xff, 0xe7, 
	0xe0, 0xc0, 0x31, 0xce, 0x08, 0xe3, 0x00, 0xc7, 0x1f, 0x4e, 0x77, 0xf9, 0xdc, 0x0f, 0xff, 0xe7, 
	0xe1, 0xc0, 0x31, 0xce, 0x18, 0xc7, 0x00, 0xc3, 0x1f, 0x6e, 0x37, 0xf0, 0x79, 0xff, 0xff, 0xef, 
	0xf3, 0xcf, 0x31, 0xcf, 0x38, 0xc7, 0x1f, 0xc3, 0x1e, 0x66, 0x37, 0xf7, 0x3f, 0xff, 0xe1, 0xef, 
	0xf3, 0xcf, 0xf1, 0xcf, 0x38, 0x8f, 0x1f, 0xc1, 0x1e, 0x71, 0x37, 0xf7, 0xb8, 0xff, 0xcc, 0xef, 
	0xf3, 0xcf, 0xf0, 0x0f, 0x38, 0x0f, 0x07, 0xc1, 0x1f, 0x7c, 0x2f, 0xf5, 0xc6, 0x3f, 0x1e, 0xdf, 
	0xf3, 0xcf, 0xf0, 0x0f, 0x38, 0x0f, 0x07, 0xc0, 0x1f, 0x8e, 0x2f, 0xf6, 0xc7, 0xcc, 0x7e, 0xdf, 
	0xf3, 0xcf, 0xf1, 0xcf, 0x38, 0x8f, 0x1f, 0xc4, 0x1f, 0x80, 0x8f, 0xf8, 0x97, 0xe1, 0xfe, 0xbf, 
	0xf3, 0xcf, 0x31, 0xcf, 0x38, 0xc7, 0x1f, 0xc4, 0x1f, 0xcb, 0x87, 0xfc, 0x34, 0xe7, 0xfe, 0xff, 
	0xe1, 0xc0, 0x31, 0xce, 0x18, 0xc7, 0x00, 0xc6, 0x1f, 0xe0, 0x03, 0xfe, 0x76, 0xcf, 0xfe, 0xff, 
	0xe0, 0xc0, 0x31, 0xce, 0x08, 0xe3, 0x00, 0xc6, 0x1f, 0xf8, 0x13, 0xfe, 0xf0, 0xdf, 0xfc, 0xff, 
	0xe0, 0xc0, 0x31, 0xce, 0x18, 0xe3, 0x00, 0xc7, 0x1f, 0xf8, 0x13, 0xfc, 0xfc, 0x3f, 0xf9, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf3, 0x67, 0xfd, 0xff, 0xff, 0xe3, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfa, 0x6f, 0xfc, 0x1f, 0xff, 0xe7, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0x67, 0xfe, 0x3f, 0xef, 0xef, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x37, 0xfe, 0xff, 0x9f, 0xef, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xb7, 0xfe, 0xee, 0x3f, 0xcf, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xbb, 0xff, 0x60, 0xff, 0x9f, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xdb, 0xff, 0x3f, 0xff, 0x3f, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xd9, 0xfb, 0x9f, 0xf8, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc1, 0xf7, 0xe3, 0x83, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe3, 0xe7, 0xf8, 0x1f, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf1, 0xef, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0xf9, 0xcf, 0x7f, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x38, 0xdc, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x98, 0xf1, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xec, 0x77, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0x7f, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x1e, 0xf0, 0x03, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

//オープニング演出時の文字列
char ichiken[][20] = {"Intuitive","Controller with","Hall-sensor","Interface","Kit for","Enhanced","Navigation"};