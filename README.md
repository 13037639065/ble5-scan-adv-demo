# linux ble5.0 advertise and scan demo.

## install dep

```
sudo apt install libncurses5-dev
sudo apt install libbluetooth-dev
```

## build

```
cc scan.c -lbluetooth -o scanner
cc advertise.c -lbluetooth -o advertise
```

## run

```
sudo ./advertise
sudo ./scan
```