## IMFlasher

Universal flasher to flash firmware in combination with [IMBootloader](https://github.com/IMProject/IMBootloader).

For the **Windows** users, STM32 Virtual COM Port Driver is needed. 
Driver could be downloaded from [google drive](https://drive.google.com/file/d/1Q3xW258Yz5Dm848b3n0GM79cMouMZZW2/view?usp=sharing)
or from [ST page](https://my.st.com/content/my_st_com/en/products/development-tools/software-development-tools/stm32-software-development-tools/stm32-utilities/stsw-stm32102.license=1620802527054.product=STSW-STM32102.version=1.5.0.html).

### Development
For development Qt 5 is needed. Recommended version is Qt 5.12.2.
The link for Qt online installer can be found here https://www.qt.io/download-open-source

### GUI
![IMFlasher_v1 2 0](https://user-images.githubusercontent.com/10188706/166103709-5e37b51f-34e5-41c7-953a-fbe97f88fc1b.gif)


### Console

Command for flashing

`./IMFlasher_Linux_v1.0.0.AppImage flash "IMLedBlink/build/stm32h7xx/IMLedBlink_stm32h7xx_signed.bin"`

![IMFlasher_console](https://user-images.githubusercontent.com/10188706/120115162-bc0fe680-c182-11eb-81ce-543e9fd1175b.gif)

