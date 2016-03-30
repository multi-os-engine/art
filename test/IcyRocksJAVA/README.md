IcyRocks-Java
===
IcyRocks-Java is a java implemented game workload based on analysis of real computation patterns.
IcyRocks is developed based on libGDX, with both Box2D and JBox2D version. With libGDX, IcyRocks can run on Android, desktop and even as an HTML5 web app. JBox2D is a java port of Box2D physics engine.

Source Files
---
There are several projects orgnized as normal libGDX projects.
    * files under `core` are game logic implementation of IcyRocks with Box2D.
    * files under `core-jbox2d` are game logic implementation of IcyRocks with JBox2D.
    * files under `android`, `desktop`, and `html` are launchers for Android, desktop and HTML5 web app.
    * files under `aurelienribon/bodyeditor` are from open source project box2d-editor at [Google Code][https://code.google.com/p/box2d-editor].

Matrics
---
    * The main metrics of this workload is AFPS on bottom-left. It means average FPS since last scenario change.
    * There are also other four metrics:
      1. FPS: instant FPS, calculated every 100ms
      2. TIME: total execution time since last scenario changed
      3. SNOW: number of snow grains in scenario. More snow grains means higher computation load.
      4. ROCK: number of icy rocks in scenario. More rocks means higher computation load.

Requirements
---

In order to use desktop version of IcyRocks-Java, java 6 or beyond is needed. You will need to have an android device with android verison equal or later than 4.4 to run the android version, which means SDK API level later or equal to 19.
The previous version of android was not guaranteed to sucessfully run this workload.

Build
---
You can build IcyRocks-Java in command line or in Eclipse.

### Box2D or JBox2D

Box2D is used by libGDX as default physical engine. We have made an JBox2D version for measurement of ART performance. In build.gradle on top level, change dependency of project `android` from `project(:core)` to `project(:core-jbox2d)` will make IcyRocks use JBox2D instead of Box2D.

### Build in command line:

    * Run `./gradlew tasks --all` to see all the tasks.
    * To build an android verison, run `./gradlew android:build`.
    * Run `./gradlew build` will build all prjects.

To find more usage of Gradle, please check Gradle [User Guide][https://docs.gradle.org/current/userguide/userguide.html].

### Build in Android Studio or Intellij Idea

    * Android SDK version should be later or euqal to 19.
    * Run `./gradlew idea` to generate Idea project files.
    * In Android Studio, choose 'Open project' and find the location of this project.

Usage
===
* Install GameWorkload.apk on your device
* Launch the GameWorkload application
* Tap the screen to change rock and snow grain numbers. Current default setting will add 10 rocks for each tap.
