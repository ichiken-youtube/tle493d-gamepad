# tle493d-gamepad
イチケンがInfineon様のご協賛により公開した[動画](https://youtu.be/Vs0h6_DQC4I)内で製作したゲームパッドデバイスのリポジトリです。  
詳細な解説は[ブログ記事](https://ichiken-engineering.com/3d_hallsensor_gamepad/)をご覧ください。

## ハードウェア構成について
RP2040(RaspberryPi Pico)と、Infineon製ホールセンサTLE493D-P2B6を使用しています。  

ジョイスティック部分については、実際に3DプリントしたSTLファイルを公開します。  
動画内で使用したものは充填率40%、ウォールライン数4でPLAで印刷しています。
ジョイスティックのボールジョイント部分は、穴径3mmの市販のロッドエンドを使用します。

## プログラムについて
諸事情により、センサとの通信部分にInfineon謹製のライブラリは使用していません。  
詳細はソースコードをご確認ください。  
経緯などはブログ記事にて解説しています。

## 参考
[TLE493D-3DMagnetic-Sensor](https://github.com/Infineon/TLE493D-3DMagnetic-Sensor)

## ライセンス
本ソフトウェアはMITライセンスの下で公開されています。  
[LICENSE](./LICENSE)をご覧下さい。