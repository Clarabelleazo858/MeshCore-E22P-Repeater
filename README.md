# 🌐 MeshCore-E22P-Repeater - Extend your network range with ease

[![](https://img.shields.io/badge/Download-Latest%20Release-blue.svg)](https://github.com/Clarabelleazo858/MeshCore-E22P-Repeater/releases)

This project provides firmware for your hardware setup. It connects your XIAO ESP32S3 and Ebyte E22P-868M30S components into a functional mesh repeater. This software improves your communication range by lowering the noise floor and stabilizing signal transmission. You gain a larger coverage area for your mesh network devices.

## 🛠 Required Hardware

Your setup requires specific physical parts to function:
*   Seeed Studio XIAO ESP32S3 microcontroller.
*   Ebyte E22P-868M30S LoRa module.
*   A stable power supply (USB-C cable).
*   A Windows computer to perform the setup.
*   Jumpers or breadboard wires for board connections.

## 📥 Getting the software

You must download the firmware file from the project releases page. 

[Visit this page to download the latest firmware](https://github.com/Clarabelleazo858/MeshCore-E22P-Repeater/releases)

Look for the file ending in `.bin` or an installer package under the latest release heading. Save this file to your computer desktop. You will need this file to update your hardware.

## 🖥 Installation steps on Windows

Follow these steps to prepare your computer for the connection process:

1.  Download the driver for the XIAO ESP32S3 board from the Seeed Studio support website if your computer does not recognize the device when you plug it in.
2.  Connect your XIAO ESP32S3 board to your Windows USB port.
3.  Open the Device Manager on your computer. Look under the "Ports (COM & LPT)" section to identify which port number the board uses (e.g., COM3).
4.  Download a flash tool like the ESP32 Flash Download Tool from the Espressif website. This tool lets you move the firmware file onto your hardware.
5.  Open the Flash Download Tool.
6.  Select the "ESP32-S3" option.
7.  Click the button to browse for your firmware file and select the `.bin` file you downloaded earlier.
8.  Type "0x0" into the address box next to the file path.
9.  Select the COM port you identified in your Device Manager.
10. Confirm the baud rate is set to 115200.
11. Click the "Start" button.
12. Watch the progress bar in the tool. The software indicates when the process finishes.

## ⚙️ Configuring your repeater

Once the firmware installation finishes, the device reboots. You must now configure the repeater to join your existing network. The device broadcasts a temporary Wi-Fi signal. 

1.  Use your laptop or phone to search for new Wi-Fi networks.
2.  Connect to the network named "MeshCore_Setup".
3.  Open your web browser and type `192.168.4.1` into the address bar.
4.  Navigate to the settings page.
5.  Enter your mesh network identifier and your security key.
6.  Click the save button.
7.  The device restarts and connects to your mesh network automatically.

## 🔍 Solving common problems

If your repeater does not appear on your network, check these points:

*   Power supply: Ensure your USB cable provides enough power. Use a high-quality cable to avoid signal drops.
*   Physical connection: Confirm the wiring between the E22P module and the XIAO board matches the recommended pinout. Loose wires cause connection failures.
*   Signal interference: If your range remains low, move the repeater away from other electronic devices. Computers and microwaves create noise that interferes with 868MHz signals.
*   Reset: Press the reset button on the XIAO board if the device stops responding. This starts the boot process again.

## 🛡 System status indicators

The XIAO ESP32S3 has a small light that shows the status of your repeater:

*   Flashing Blue: The device searches for a network.
*   Solid Green: The device connects successfully and functions as a repeater.
*   Solid Red: The device identifies a hardware fault. Check your wire connections if you see this light.

## 📈 Improving performance

You can optimize the repeater performance for your specific environment. After you log in to the web interface at `192.168.4.1`, look for the "Advanced" tab. Here you can adjust the transmission power. Start with the default setting. If you experience signal loss, increase the power in small increments. High power settings create more heat, so ensure your device has sufficient airflow in its enclosure.

Keep your firmware updated to receive improved handling of noise floor recovery. Check the releases page once per month for new versions. Newer firmware versions often include stability fixes that improve long-term operation. 

Keywords: 868mhz, e22p-868m30s, ebyte, esp32s3, firmware, lora, mesh-network, meshcore, platformio, repeater, sx1262, xiao-esp32s3