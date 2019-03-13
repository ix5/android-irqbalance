# irqbalance daemon for Android
 
Compiles on bionic.
 
Split out from [enesuzun2002's samsung zero devicetree](https://github.com/enesuzun2002/android_device_samsung_zero-common).
 
## How to use
Edit `irqbalance.conf`, then:
```
PRODUCT_PACKAGES += irqbalance
PRODUCT_COPY_FILES += \
    irqbalance.conf:$(PRODUCT_COPY_OUT_VENDOR)/etc/irqbalance.conf
```
 
## License
Apache 2, see [LICENSE.md](LICENSE.md).
