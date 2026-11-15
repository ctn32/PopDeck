PopDeck Android (WebView app)
=============================
Opens the PopDeck phone-remote page of your PC like a native app. Address and PIN are remembered.
On first launch the "Show on" switch defaults to "Phone"; change it from the top of the page (PC / Phone / Both).

BUILD THE APK
1) Install Android Studio (https://developer.android.com/studio). Put this folder in a path WITHOUT non-ASCII
   characters (e.g. C:\PopDeckAndroid).
2) Android Studio > Open > select this folder. On the first sync it downloads Gradle and the SDK (internet needed).
3) Menu: Build > Build Bundle(s) / APK(s) > Build APK(s).
4) The APK is created at: app/build/outputs/apk/debug/app-debug.apk
5) Copy it to your phone, open it and allow "install unknown apps". Or enable USB debugging and press Run in Studio.

USAGE
- Open the app, enter the address shown in PopDeck's "Phone Remote" tab (e.g. 192.168.1.25:8080) and the PIN.
- Back button: returns to the connection screen (to change address/PIN).
- The screen stays on while the app is open. Tap an image to dismiss it.

LIMITATION
- Images play inside the app; they cannot be drawn over other apps or games (that would need a separate overlay service).
