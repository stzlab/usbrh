## 概要
[Strawberry Linux Co.,Ltd.](http://strawberry-linux.com/)の温度計・湿度計モジュール(USBRH)をLinux環境で扱うためのプログラムです。

カーネルのアップデートの影響を受けにくい[HIDAPI](https://github.com/signal11/hidapi)により実装しています。

## 開発環境
Ubuntu Desktop 20.04 LTS

## ビルド
     $ apt install -y build-essential libhidapi-dev
     $ make

make installは未実装です。

一般ユーザーで動作させる場合は、99-hidraw-usbrh.rulesを/etc/udev/rules.d/に配置するなどして、該当のhidrawデバイスにアクセス許可を与えてください。

## 使用方法
usbrh [-dlfVRGH]

    (オプションなし)				# 日付・時刻、温度・湿度を表示
    -d  : Enable debugging			# デバッグ情報の表示
    -h  : Show usage			# 使用方法の表示
    -l  : Show device list             	# 接続デバイスの一覧表示
    -sn : Specify device number (n=0:all)	# 複数接続時のデバイス指定(n=0:すべて)
    -V  : Show firmware version		# ファームウェアバージョンの表示
    -Rn : Control Red   LED (0:off, 1:on)	# LED(赤)のON/OFF
    -Gn : Control Green LED (0:off, 1:on)	# LED(緑)のON/OFF
    -Hn : Control Heater    (0:off, 1:on)	# 診断用のヒーターのON/OFF

## 実行例
     $ ./usbrh
     tm:2020/10/10-10:20:20 tc1:22.58 rh1:64.09

## 謝辞
USBRHの制御方法については、[USBRH on Linux](https://github.com/m24o8l/usbrh)を参考にさせていただきました。
