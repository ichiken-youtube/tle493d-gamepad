# tle493d-gamepad
イチケンがInfineon様のご協賛により公開した[動画](https://www.youtube.com/watch?v=XXXXXXXX)内で製作したゲームパッドデバイスのリポジトリです。  
詳細は[ブログ記事](https://ichiken-engineering.com/XXXXXXXX/)をご覧ください。

## ハードウェア構成について
RP2040(RaspberryPi Pico)と、Infineon製ホールセンサTLE493D-P2B6を使用しています。  
ジョイスティック部分については、実際に3DプリントしたSTLファイルを公開します。  
動画内で使用したものは充填率40%、ウォールライン数4で印刷しています。

## プログラムについて
諸事情により、センサとの通信部分にInfineon謹製のライブラリは使用していません。  
ブログの解説記事やソースコードをご覧ください。

## 参考
[TLE493D-3DMagnetic-Sensor](https://github.com/Infineon/TLE493D-3DMagnetic-Sensor)