import 'dart:convert';
import 'dart:typed_data';

import 'package:flutter/material.dart';
import 'package:flutter/cupertino.dart';
import 'package:flutter_bluetooth_serial/flutter_bluetooth_serial.dart';

void main() {
  runApp(ICXSApp());
}

class ICXSApp extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Interval Cam XV Setup App.',
      theme: ThemeData(
        primarySwatch: Colors.blue,
      ),
      home: ICXSAppHome(title: 'Interval Cam XV Setup App.'),
    );
  }
}

class ICXSAppHome extends StatefulWidget {
  ICXSAppHome({Key? key, required this.title}) : super(key: key);
  final String title;

  @override
  _ICXSAppHomeState createState() => _ICXSAppHomeState();
}

class _ICXSAppHomeState extends State<ICXSAppHome> {
  static const String defaultPASS = "********";
  static const TextStyle titleTextStyle =
      TextStyle(fontWeight: FontWeight.bold);

  bool _btGet = false;
  bool _btSet = false;
  String _btSetting = "";
  bool _connected = false;
  FlutterBluetoothSerial bluetooth = FlutterBluetoothSerial.instance;
  late BluetoothDevice esp32cam;
  late BluetoothConnection connection;

  List<DropdownMenuItem<int>> _camFrameSizes = [];

  double eepBrightness = 0.0;
  double eepIntervalSec = 60;
  double eppCriteria = 3000;
  int eepCamFrameSize = 0;
  bool eepWifi = true;
  bool eepSD = false;
  String eepWifiSSID = "";
  String eepWifiPASS = defaultPASS;
  String eepWifiURL = "";

  late TextEditingController _ctrlWifiSSID;
  late TextEditingController _ctrlWifiPASS;
  late TextEditingController _ctrlWifiURL;

  @override
  void initState() {
    super.initState();
    _ctrlWifiSSID = TextEditingController();
    _ctrlWifiSSID.value = TextEditingValue(text: eepWifiSSID);
    _ctrlWifiPASS = TextEditingController();
    _ctrlWifiPASS.value = TextEditingValue(text: defaultPASS);
    _ctrlWifiURL = TextEditingController();
    _ctrlWifiURL.value = TextEditingValue(text: eepWifiSSID);
  }

  Future<bool> searchDevice() async {
    print("[DEBUG] Searching Device");
    setState(() {
      _connected = false;
    });

    List<BluetoothDevice> devices = [];
    devices = await bluetooth.getBondedDevices();
    for (BluetoothDevice d in devices) {
      print("[DEBUG]  Device: ${d.name}");
      if (d.name == null) {
        continue;
      }
      if (d.name == "IntervalCamXV") {
        print("[DEBUG] Find IntervalCamXV");
        this.esp32cam = d;
        this.connection = await BluetoothConnection.toAddress(esp32cam.address);
        this.connection.input!.listen((Uint8List data) {
          String dataStr = btListen(data);
          if (_btGet) {
            getSetting(dataStr);
          } else if (_btSet) {
            setSetting(dataStr);
          }
        }).onDone(() {
          print("[DEBUG] sendBTMulti done");
          btDone();
        });
        setState(() {
          print("[DEBUG] Connected");
          this._connected = true;
        });
        // Load From ESP32
        receiveSetting();
        return true;
      }
    }
    return false;
  }

  Future<void> sendBT(int command) async {
    print("[DEBUG] sendBT");
    sendBTMulti([command]);
  }

  Future<void> sendAndReceive(String command) async {
    print("[DEBUG] sendAndReceive: " + command);
    sendBTMulti(ascii.encode(command));
  }

  Future<void> sendBTMulti(List<int> command) async {
    print("[DEBUG] sendBTMulti");
    if (this.connection.isConnected) {
      print("[DEBUG] sendBTMulti output Add");
      this.connection.output.add(Uint8List.fromList(command));
    }
  }

  String btListen(Uint8List data) {
    String dataStr = ascii.decode(data);
    print("[DEBUG] btListen: " + dataStr);
    return dataStr;
  }

  bool getSetting(String data) {
    _btSetting += data;
    print("[DEBUG] getSetting: " + data);
    if (data.substring(data.length - 3, data.length - 2) == ";") {
      parseToSetSetting(_btSetting);
      setState(() {
        _btGet = false;
      });
      return true;
    }
    print("[DEBUG] getSetting: waiting...");
    return false;
  }

  void parseToSetSetting(String res) {
    print("[DEBUG] parseToSetSetting: " + res);
    int idxBrightness = getReceiveIdx(res, "#BRIGHTNESS:");
    int idxBrightnessE = getReceiveIdxE(res, ",CDS_CRITERIA:");
    int idxCdsCriteria = getReceiveIdx(res, ",CDS_CRITERIA:");
    int idxCdsCriteriaE = getReceiveIdxE(res, ",SLEEP_TIME:");
    int idxSleepTime = getReceiveIdx(res, ",SLEEP_TIME:");
    int idxSleepTimeE = getReceiveIdxE(res, ",FRAMESIZE:");
    int idxFrameSize = getReceiveIdx(res, ",FRAMESIZE:");
    int idxFrameSizeE = getReceiveIdxE(res, ",USESD:");
    int idxUseSD = getReceiveIdx(res, ",USESD:");
    int idxUseSDE = getReceiveIdxE(res, ",USEWIFI:");
    int idxUseWifi = getReceiveIdx(res, ",USEWIFI:");
    int idxUseWifiE = getReceiveIdxE(res, ",WIFISSID:");
    int idxWifiSSID = getReceiveIdx(res, ",WIFISSID:");
    int idxWifiSSIDE = getReceiveIdxE(res, ",WIFIPASS:");
    // int idxWifiPass = getReceiveIdx(res, ",WIFIPASS:");
    // int idxWifiPassE = getReceiveIdxE(res, ",WIFIURL:");
    int idxWifiURL = getReceiveIdx(res, ",WIFIURL:");
    int idxWifiURLE = res.length - 3;
    setState(() {
      eepBrightness = getReceiveInt(res, idxBrightness, idxBrightnessE) / 10;
      eppCriteria = getReceiveInt(res, idxCdsCriteria, idxCdsCriteriaE) / 1;
      eepIntervalSec = getReceiveInt(res, idxSleepTime, idxSleepTimeE) / 1;
      eepCamFrameSize = getReceiveInt(res, idxFrameSize, idxFrameSizeE);
      eepSD = getReceiveBool(res, idxUseSD, idxUseSDE);
      eepWifi = getReceiveBool(res, idxUseWifi, idxUseWifiE);
      eepWifiSSID = getReceiveStr(res, idxWifiSSID, idxWifiSSIDE);
      // eepWifiPASS = getReceiveStr(res, idxWifiPass, idxWifiPassE);
      eepWifiURL = getReceiveStr(res, idxWifiURL, idxWifiURLE);

      _ctrlWifiSSID.text = eepWifiSSID;
      _ctrlWifiPASS.text = defaultPASS;
      _ctrlWifiURL.text = eepWifiURL;

      print("[DEBUG] eepBrightness:" + eepBrightness.toString());
      print("[DEBUG] eppCriteria:" + eppCriteria.toString());
      print("[DEBUG] eepIntervalSec:" + eepIntervalSec.toString());
      print("[DEBUG] eepCamFrameSize:" + eepCamFrameSize.toString());
      print("[DEBUG] eepSD:" + eepSD.toString());
      print("[DEBUG] eepWifi:" + eepWifi.toString());
      print("[DEBUG] eepWifiSSID:" + eepWifiSSID.toString());
      print("[DEBUG] eepWifiURL:" + eepWifiURL.toString());
    });
  }

  int getReceiveIdx(String data, String command) {
    int idx = data.indexOf(command);
    if (idx == -1) {
      return -1;
    }
    return idx + command.length;
  }

  int getReceiveIdxE(String data, String command) {
    int idx = data.indexOf(command);
    if (idx == -1) {
      return -1;
    }
    return idx;
  }

  int getReceiveInt(String data, int s, int e) {
    if (s == -1 || e == -1) {
      return 0;
    }
    String val = data.substring(s, e);
    print("[DEBUG] getReceiveInt value: " + val);
    if (val == "nan") {
      return 0;
    }
    return double.parse(val).round();
  }

  bool getReceiveBool(String data, int s, int e) {
    int val = getReceiveInt(data, s, e);
    return val == 1;
  }

  String getReceiveStr(String data, int s, int e) {
    if (s == -1 || e == -1) {
      return "";
    }
    return data.substring(s, e);
  }

  bool setSetting(String data) {
    if (data == "OK:COMMIT") {
      setState(() {
        _btSet = false;
      });
      return true;
    }
    return false;
  }

  void btDone() {
    print('Bluetooth listen Done');
  }

  void receiveSetting() {
    if (_btGet || _btSet) {
      return;
    }
    print("[DEBUG] receiveSetting");
    setState(() {
      _btGet = true;
    });
    _btSetting = "";
    sendAndReceive("#GET:SETTINGS;");
  }

  void waitBT() async {
    print("[DEBUG] waitBT");
    while (true) {
      await new Future.delayed(new Duration(milliseconds: 100));
    }
  }

  void commitSetting() {
    if (_btGet || _btSet) {
      return;
    }
    setState(() {
      _btSet = true;
    });
    sendAndReceive(
        "#SET:BRIGHTNESS/" + (eepBrightness * 10).round().toString() + ";");
    sendAndReceive("#SET:CDS_CRITERIA/" + eppCriteria.round().toString() + ";");
    sendAndReceive(
        "#SET:SLEEP_TIME/" + eepIntervalSec.round().toString() + ";");
    sendAndReceive("#SET:FRAMESIZE/" + eepCamFrameSize.toString() + ";");
    String eepSDStr = (eepSD) ? "1" : "0";
    sendAndReceive("#SET:USESD/" + eepSDStr + ";");
    String eepWifiStr = (eepWifi) ? "1" : "0";
    sendAndReceive("#SET:USEWIFI/" + eepWifiStr + ";");
    String wifiSSID = _ctrlWifiSSID.text;
    sendAndReceive("#SET:WIFISSID/" + wifiSSID + ";");
    String wifiPASS = _ctrlWifiPASS.text;
    if (wifiPASS != defaultPASS) {
      sendAndReceive("#SET:WIFIPASS/" + wifiPASS + ";");
    }
    String wifiURL = _ctrlWifiURL.text;
    sendAndReceive("#SET:WIFIURL/" + wifiURL + ";");
    sendAndReceive("#SET:COMMIT;");
  }

  void resetFileCount() {
    if (_btGet || _btSet) {
      return;
    }
    sendAndReceive("#SET:RESET;");
  }

  void testCapture() {
    if (_btGet || _btSet) {
      return;
    }
    sendAndReceive("#CMD:TEST;");
  }

  void reboot() {
    if (_btGet || _btSet) {
      return;
    }
    sendAndReceive("#CMD:REBOOT;");
  }

  void camFrameCombosBuild() {
    if (_camFrameSizes.isNotEmpty) {
      return;
    }
    _camFrameSizes
      ..add(DropdownMenuItem(
        child: Text(
          'DEFAULT(640x480)',
        ),
        value: 0,
      ))
      ..add(DropdownMenuItem(
        child: Text(
          'QVGA(320x240)',
        ),
        value: 1,
      ))
      ..add(DropdownMenuItem(
        child: Text(
          'CIF(400x296)',
        ),
        value: 2,
      ))
      ..add(DropdownMenuItem(
        child: Text(
          'VGA(640x480)',
        ),
        value: 3,
      ))
      ..add(DropdownMenuItem(
        child: Text(
          'SVGA(800x600)',
        ),
        value: 4,
      ))
      ..add(DropdownMenuItem(
        child: Text(
          'XGA(1024x768)',
        ),
        value: 5,
      ))
      ..add(DropdownMenuItem(
        child: Text(
          'SXGA(1280x1024)',
        ),
        value: 6,
      ))
      ..add(DropdownMenuItem(
        child: Text(
          'UXGA(1600x1200)',
        ),
        value: 7,
      ));
  }

  @override
  Widget build(BuildContext context) {
    final Size size = MediaQuery.of(context).size;
    camFrameCombosBuild();
    return Scaffold(
      appBar: AppBar(
        title: Text(widget.title),
      ),
      body: SingleChildScrollView(
        child: Center(
          child: Stack(
            children: <Widget>[
              Visibility(
                visible: _connected == false,
                child: Padding(
                  padding: EdgeInsets.symmetric(vertical: 10, horizontal: 10),
                  child: Column(
                    children: [
                      Container(
                        height: 70,
                        alignment: Alignment.center,
                        child: Center(
                          child: Text(
                            "Interval Cam XV App.",
                            style: TextStyle(fontSize: 36),
                          ),
                        ),
                      ),
                      Container(
                        width: size.width / 2,
                        height: size.width / 2,
                        decoration: BoxDecoration(
                            shape: BoxShape.rectangle,
                            image: DecorationImage(
                                fit: BoxFit.fill,
                                image:
                                    AssetImage("assets/camera_niganrefu.png"))),
                      ),
                      Container(
                        height: 100,
                        alignment: Alignment.center,
                        child: Text("Not Connected the device yet.\n" +
                            "Pair with 'IntervalCamXV' \n" +
                            "using Smart phone Bluetooth Setting at first.\n\n" +
                            "For Assemble, please head to github. \n" +
                            "https://github.com/koyaaaaaan/interval-cam-xv "),
                      ),
                      Container(
                        height: 30,
                      ),
                      ElevatedButton(
                        style: ButtonStyle(
                            padding:
                                MaterialStateProperty.all(EdgeInsets.all(25)),
                            backgroundColor:
                                MaterialStateProperty.all<Color>(Colors.green)),
                        onPressed: () {
                          searchDevice();
                        },
                        child: Text("Bluetooth Connect"),
                      ),
                    ],
                  ),
                ),
              ),
              Visibility(
                visible: _connected == true && (_btGet || _btSet),
                child: Container(
                  width: size.width,
                  height: size.height * 2,
                  color: Colors.black45,
                  child: Center(
                    child: Column(
                      mainAxisAlignment: MainAxisAlignment.start,
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: <Widget>[
                        CircularProgressIndicator(
                            valueColor: new AlwaysStoppedAnimation<Color>(
                                Colors.black45))
                      ],
                    ),
                  ),
                ),
              ),
              Visibility(
                visible: _connected,
                child: Padding(
                  padding: EdgeInsets.symmetric(vertical: 10, horizontal: 10),
                  child: Column(
                    children: [
                      Row(
                        children: [
                          Text(
                            "Settings",
                            style: TextStyle(
                                fontSize: 20, fontWeight: FontWeight.bold),
                          ),
                        ],
                      ),
                      Row(
                        mainAxisAlignment: MainAxisAlignment.spaceBetween,
                        crossAxisAlignment: CrossAxisAlignment.center,
                        children: [
                          Text(
                            "Camera Brightness (" +
                                eepBrightness.toString() +
                                ")",
                            style: titleTextStyle,
                          ),
                          Slider(
                            value: eepBrightness,
                            min: -2,
                            max: 2,
                            divisions: 4,
                            label: eepBrightness.toString(),
                            onChanged: (double value) {
                              setState(() {
                                eepBrightness = ((value * 10).round() / 10);
                              });
                            },
                          ),
                        ],
                      ),
                      Row(
                          mainAxisAlignment: MainAxisAlignment.end,
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            Text("-2.0:Dark  <  2.0:Bright"),
                          ]),
                      Row(
                        mainAxisAlignment: MainAxisAlignment.spaceBetween,
                        crossAxisAlignment: CrossAxisAlignment.center,
                        children: [
                          Text(
                            "Shot Interval (" +
                                eepIntervalSec.round().toString() +
                                "sec)",
                            style: titleTextStyle,
                          ),
                          Slider(
                            value: eepIntervalSec,
                            min: 10,
                            max: 600,
                            divisions: 59,
                            label: eepIntervalSec.round().toString(),
                            onChanged: (double value) {
                              setState(() {
                                eepIntervalSec = value;
                              });
                            },
                          ),
                        ],
                      ),
                      Row(
                          mainAxisAlignment: MainAxisAlignment.end,
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            Text("10 sec  <  600 sec"),
                          ]),
                      Row(
                        mainAxisAlignment: MainAxisAlignment.spaceBetween,
                        crossAxisAlignment: CrossAxisAlignment.center,
                        children: [
                          Text(
                            "Flush Light Sensitivity (" +
                                eppCriteria.round().toString() +
                                ")",
                            style: titleTextStyle,
                          ),
                          Slider(
                            value: eppCriteria,
                            min: 0,
                            max: 4100,
                            divisions: 41,
                            label: eppCriteria.round().toString(),
                            onChanged: (double value) {
                              setState(() {
                                eppCriteria = value;
                              });
                            },
                          ),
                        ],
                      ),
                      Row(
                          mainAxisAlignment: MainAxisAlignment.end,
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            Text("Work on Brighter  <  Work on Darker"),
                          ]),
                      Row(
                          mainAxisAlignment: MainAxisAlignment.end,
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            Text("0:always  4100:never"),
                          ]),
                      Row(
                        mainAxisAlignment: MainAxisAlignment.spaceBetween,
                        crossAxisAlignment: CrossAxisAlignment.center,
                        children: [
                          Text(
                            "CAM Frame Size",
                            style: titleTextStyle,
                          ),
                          DropdownButton(
                            items: _camFrameSizes,
                            value: eepCamFrameSize,
                            onChanged: (value) => {
                              setState(() {
                                eepCamFrameSize = int.parse(value.toString());
                              }),
                            },
                          ),
                        ],
                      ),
                      Row(
                          mainAxisAlignment: MainAxisAlignment.end,
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            Text("Bigger size can get error to reboot."),
                          ]),
                      Row(
                        mainAxisAlignment: MainAxisAlignment.spaceBetween,
                        crossAxisAlignment: CrossAxisAlignment.center,
                        children: [
                          Text(
                            "SD Enabled",
                            style: titleTextStyle,
                          ),
                          Switch(
                            value: eepSD,
                            onChanged: (bool value) {
                              setState(() {
                                eepSD = value;
                              });
                            },
                          ),
                        ],
                      ),
                      Row(
                        mainAxisAlignment: MainAxisAlignment.spaceBetween,
                        crossAxisAlignment: CrossAxisAlignment.center,
                        children: [
                          Text(
                            "Wifi Enabled",
                            style: titleTextStyle,
                          ),
                          Switch(
                            value: eepWifi,
                            onChanged: (bool value) {
                              setState(() {
                                eepWifi = value;
                              });
                            },
                          ),
                        ],
                      ),
                      Visibility(
                        visible: eepWifi,
                        child: Column(
                          children: [
                            Container(
                              height: 20,
                            ),
                            Row(
                              mainAxisAlignment: MainAxisAlignment.spaceBetween,
                              crossAxisAlignment: CrossAxisAlignment.center,
                              children: [
                                Text(
                                  "Wifi SSID",
                                  style: titleTextStyle,
                                ),
                              ],
                            ),
                            TextField(
                              controller: _ctrlWifiSSID,
                              maxLength: 64,
                            ),
                            Row(
                              mainAxisAlignment: MainAxisAlignment.spaceBetween,
                              crossAxisAlignment: CrossAxisAlignment.center,
                              children: [
                                Text(
                                  "Wifi PASS",
                                  style: titleTextStyle,
                                ),
                              ],
                            ),
                            TextField(
                              controller: _ctrlWifiPASS,
                              obscureText: true,
                              maxLength: 64,
                            ),
                            Row(
                              mainAxisAlignment: MainAxisAlignment.spaceBetween,
                              crossAxisAlignment: CrossAxisAlignment.center,
                              children: [
                                Text(
                                  "URL of Post Image",
                                  style: titleTextStyle,
                                ),
                              ],
                            ),
                            TextField(
                              maxLength: 500,
                              controller: _ctrlWifiURL,
                            ),
                          ],
                        ),
                      ),
                      Container(
                        height: 20,
                      ),
                      Row(
                        mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                        crossAxisAlignment: CrossAxisAlignment.center,
                        children: [
                          ElevatedButton(
                              style: ButtonStyle(
                                  padding: MaterialStateProperty.all(
                                      EdgeInsets.symmetric(
                                          horizontal: 30, vertical: 10)),
                                  backgroundColor:
                                      MaterialStateProperty.all<Color>(
                                          Colors.blue)),
                              onPressed: () {
                                commitSetting();
                              },
                              child: Text(
                                "Save",
                                style: TextStyle(fontSize: 30),
                              )),
                        ],
                      ),
                      Container(
                        height: 60,
                      ),
                      Row(
                        children: [
                          Text(
                            "Other Actions",
                            style: TextStyle(
                                fontSize: 20, fontWeight: FontWeight.bold),
                          ),
                        ],
                      ),
                      Row(
                        mainAxisAlignment: MainAxisAlignment.spaceBetween,
                        crossAxisAlignment: CrossAxisAlignment.center,
                        children: [
                          Text(
                            "Reload Setting from IntervalCamXV",
                            style: titleTextStyle,
                          ),
                          ElevatedButton(
                            style: ButtonStyle(
                                backgroundColor:
                                    MaterialStateProperty.all<Color>(
                                        Colors.green)),
                            onPressed: () {
                              receiveSetting();
                            },
                            child: Text("Load"),
                          ),
                        ],
                      ),
                      Row(
                        mainAxisAlignment: MainAxisAlignment.spaceBetween,
                        crossAxisAlignment: CrossAxisAlignment.center,
                        children: [
                          Text(
                            "Run Test Capture",
                            style: titleTextStyle,
                          ),
                          ElevatedButton(
                            style: ButtonStyle(
                                backgroundColor:
                                    MaterialStateProperty.all<Color>(
                                        Colors.blue)),
                            onPressed: () {
                              testCapture();
                            },
                            child: Text("Test"),
                          ),
                        ],
                      ),
                      Row(
                        mainAxisAlignment: MainAxisAlignment.spaceBetween,
                        crossAxisAlignment: CrossAxisAlignment.center,
                        children: [
                          Text(
                            "Reboot IntervalCamXV",
                            style: titleTextStyle,
                          ),
                          ElevatedButton(
                              style: ButtonStyle(
                                  backgroundColor:
                                      MaterialStateProperty.all<Color>(
                                          Colors.red)),
                              onPressed: () {
                                reboot();
                                setState(() {
                                  this._connected = false;
                                });
                              },
                              child: Text("Boot")),
                        ],
                      ),
                    ],
                  ),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}
