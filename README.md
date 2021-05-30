## IMFlasher

Universal flasher to flash firmware in combination with [IMBootloader](https://github.com/IMProject/IMBootloader).

For the **Windows** users, STM32 Virtual COM Port Driver is needed. 
Driver could be downloaded from [google drive](https://drive.google.com/file/d/1Q3xW258Yz5Dm848b3n0GM79cMouMZZW2/view?usp=sharing)
or from [ST page](https://my.st.com/content/my_st_com/en/products/development-tools/software-development-tools/stm32-software-development-tools/stm32-utilities/stsw-stm32102.license=1620802527054.product=STSW-STM32102.version=1.5.0.html).

### Development
For development Qt Creator 5.14.x is needed. The last supported version is 5.14.2


### GUI
![image](https://user-images.githubusercontent.com/10188706/118359122-41d84300-b582-11eb-8730-15c904fda4ee.png)

![image](https://user-images.githubusercontent.com/10188706/118359153-73e9a500-b582-11eb-90b4-0a5fc73e628d.png)

### Console

Command for flashing

`./IMFlasher_Linux_v1.0.0.AppImage flash "IMLedBlink/build/stm32h7xx/IMLedBlink_stm32h7xx_signed.bin"`

![IMFlasher_console](https://user-images.githubusercontent.com/10188706/120115162-bc0fe680-c182-11eb-81ce-543e9fd1175b.gif)

