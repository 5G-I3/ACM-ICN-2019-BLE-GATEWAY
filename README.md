# ACM-ICN-2019-BLE-GATEWAY
Code and documentation to reproduce the demo "NDN meets BLE: A Transparent Gateway for Opening NDN-over-BLE Networks to your Smart phone" published in Proc. of ACM ICN 2019

## Components
- Smart Watch CK12 (sensor)
- RuuviTag (forwarder)
- nrf52dk boards (gateway)
- Laptop
- Smart phone

## Preparations
- Charge Smart Watch.
- Make sure coin cell battery for RuuviTag has power.
- Install nRF Connect on your Smart phone.
- Have the MAC addresses of the forwarder and sensor at hand.

## Setup
- Remove battery protector from RuuviTag to power it.
- Connect a micro USB cable to the nrf52dk board (gateway).
- Open terminal to access shell of the gateway (`make term`).
- Make sure you have access to the RIOT shell (`help`).

## Demo Procedure
- In nRF Connect, run scanner and show advertising of "autoconn" and "Gatt-to-NDN Gateway".
- Note: Yet, there is no BLE connection because the gateway has an empty whitelist.
- Whitelist MAC address of the forwarder on the gateway (`wl xx:xx:xx:xx:xx:xx`).
- In the nRF Connect App, connect to the "Gatt-to-NDN Gateway" node. Among others, it should indicate a heart rate service.
- Subscribe to the heart rate service and see the return values change.
- Note: Yet, the Smart Watch simply returns random values.
- After (multi-hop) connection establishment you should see periodic requests of heart rate values on the gateway terminal which look like:

```
> wl e3:1b:65:46:ae:f0
whitelisting E3:1B:65:46:AE:F0 now
> ifconfig
  ifconfig
 Iface  5  HWaddr: C0:46:49:BA:C9:50
           L2-PDU:1280 Source address length: 6

[autoconn] DISABLED
[autoconn] SCAN success, initiating connection
[autoconn] CONNECTED as master 0
[autoconn] ENBALED
[NOTIFY_HRS] enabled
[NOTIFY_HRS] sending interest
interest to name: /icn19/watch/hrs/1
[NDN] received Content (size 2)
[NOTIFY_HRS] send new datum: 107
[NOTIFY_HRS] sending interest
interest to name: /icn19/watch/hrs/2
```

- Next, check out the "Unknown Service". It allows you to send Interest requests.
- Subscribe to responses of that service and then click "write value".
- Eventually set the text field option the "string".
- Send "/foo" and you should get "Bar!" in return.
- Send "/Hello" and you should get "World!" in return.
- Next to the on-demand heart rate values, "/foo" and "/Hello" are the only permanent contents
in the content store of the Smart Watch.