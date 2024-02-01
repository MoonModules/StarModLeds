/*
   @title     StarMod
   @file      AppEffects.h
   @date      20240114
   @repo      https://github.com/ewowi/StarMod
   @Authors   https://github.com/ewowi/StarMod/commits/main
   @Copyright (c) 2024 Github StarMod Commit Authors
   @license   GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
   @license   For non GPL-v3 usage, commercial licenses must be purchased. Contact moonmodules@icloud.com
*/

#ifdef USERMOD_WLEDAUDIO
  #include "../User/UserModWLEDAudio.h"
#endif
#ifdef USERMOD_E131
  #include "../User/UserModE131.h"
#endif

uint8_t gHue = 0; // rotating "base color" used by many of the patterns
unsigned long call = 0; //not used at the moment (don't use in effect calculations)
unsigned long now = millis();
CRGBPalette16 palette = PartyColors_p;

//StarMod implementation of segment.data
class SharedData {
  private:
    byte *data;
    uint16_t index = 0;
    uint16_t bytesAllocated = 0;

  public:
  SharedData() {
    bytesAllocated = 1024;
    data = (byte*) malloc(bytesAllocated); //start with 100 bytes
  }

  void clear() {
    memset(data, 0, bytesAllocated);
  }

  void allocate(size_t size) {
    index = 0;
    if (size > bytesAllocated) {
      USER_PRINTF("realloc %d %d %d\n", index, size, bytesAllocated);
      data = (byte*)realloc(data, size);
      bytesAllocated = size;
    }
  }

  template <typename Type>
  Type* bind(int length = 1) {
    Type* returnValue = reinterpret_cast<Type*>(data + index);
    index += length * sizeof(Type); //add consumed amount of bytes, index is next byte which will be pointed to
    if (index > bytesAllocated) {
      USER_PRINTF("bind too big %d %d\n", index, bytesAllocated);
      return nullptr;
    }
    return returnValue;
  }

  bool allocated() {
    if (index>bytesAllocated) {
      USER_PRINTF("not all variables bound %d %d\n", index, bytesAllocated);
      return false;
    }
    return true;
  }

} sharedData;

//should not contain variables/bytes to keep mem as small as possible!!
class Effect {
public:
  virtual const char * name() {return nullptr;}

  virtual void setup(Leds &leds) {}

  virtual void loop(Leds &leds) {}

  virtual bool controls(JsonObject parentVar, uint8_t rowNr) {return false;}

  void addPalette(JsonObject parentVar, uint8_t rowNr) {
    JsonObject currentVar = ui->initSelect(parentVar, "pal", 4, false, [](JsonObject var) { //uiFun.
      web->addResponse(var["id"], "label", "Palette");
      web->addResponse(var["id"], "comment", "Colors");
      JsonArray select = web->addResponseA(var["id"], "options");
      select.add("CloudColors");
      select.add("LavaColors");
      select.add("OceanColors");
      select.add("ForestColors");
      select.add("RainbowColors");
      select.add("RainbowStripeColors");
      select.add("PartyColors");
      select.add("HeatColors");
    }, [](JsonObject var, uint8_t rowNr) { //chFun
      switch (mdl->getValue(var, rowNr).as<uint8_t>()) {
        case 0: palette = CloudColors_p; break;
        case 1: palette = LavaColors_p; break;
        case 2: palette = OceanColors_p; break;
        case 3: palette = ForestColors_p; break;
        case 4: palette = RainbowColors_p; break;
        case 5: palette = RainbowStripeColors_p; break;
        case 6: palette = PartyColors_p; break;
        case 7: palette = HeatColors_p; break;
        default: palette = PartyColors_p; break;
      }
    });
    currentVar["stage"] = true;
  }
};

class SolidEffect: public Effect {
public:
  const char * name() {
    return "Solid 1D";
  }
  void loop(Leds &leds) {
    uint8_t red = mdl->getValue("Red");
    uint8_t green = mdl->getValue("Green");
    uint8_t blue = mdl->getValue("Blue");
    CRGB color = CRGB(red, green, blue);
    leds.fill_solid(color);
  }
  bool controls(JsonObject parentVar, uint8_t rowNr) {
    ui->initSlider(parentVar, "Red", 182);
    ui->initSlider(parentVar, "Green", 15);
    ui->initSlider(parentVar, "Blue", 98);
    return true;
  }
};

class RainbowEffect: public Effect {
public:
  const char * name() {
    return "Rainbow 1D";
  }
  void loop(Leds &leds) {
    // FastLED's built-in rainbow generator
    leds.fill_rainbow(gHue, 7);
  }
};

class RainbowWithGlitterEffect:public RainbowEffect {
public:
  const char * name() {
    return "Rainbow with glitter 1D";
  }
  void loop(Leds &leds) {
    // built-in FastLED rainbow, plus some random sparkly glitter
    RainbowEffect::loop(leds);
    addGlitter(leds, 80);
  }
  void addGlitter(Leds &leds, fract8 chanceOfGlitter) 
  {
    if( random8() < chanceOfGlitter) {
      leds[ random16(leds.nrOfLeds) ] += CRGB::White;
    }
  }
};

class SinelonEffect: public Effect {
public:
  const char * name() {
    return "Sinelon 1D";
  }
  void loop(Leds &leds) {
    // a colored dot sweeping back and forth, with fading trails
    leds.fadeToBlackBy(20);
    int pos = beatsin16( mdl->getValue("BPM").as<int>(), 0, leds.nrOfLeds-1 );
    // leds[pos] += CHSV( gHue, 255, 192);
    leds[pos] = leds.getPixelColor(pos) + CHSV( gHue, 255, 192);
    // CRGB x = leds[pos];
  }
  bool controls(JsonObject parentVar, uint8_t rowNr) {
    ui->initSlider(parentVar, "BPM", 60);
    return true;
  }
}; //Sinelon

//https://www.perfectcircuit.com/signal/difference-between-waveforms
class RunningEffect: public Effect {
public:
  const char * name() {
    return "Running 1D";
  }
  void loop(Leds &leds) {
    // a colored dot sweeping back and forth, with fading trails
    leds.fadeToBlackBy(mdl->getValue("fade").as<int>()); //physical leds
    int pos = map(beat16( mdl->getValue("BPM").as<int>()), 0, uint16_t(-1), 0, leds.nrOfLeds-1 ); //instead of call%leds.nrOfLeds
    // int pos2 = map(beat16( mdl->getValue("BPM").as<int>(), 1000), 0, uint16_t(-1), 0, leds.nrOfLeds-1 ); //one second later
    leds[pos] = CHSV( gHue, 255, 192); //make sure the right physical leds get their value
    // leds[leds.nrOfLeds -1 - pos2] = CHSV( gHue, 255, 192); //make sure the right physical leds get their value
  }
  bool controls(JsonObject parentVar, uint8_t rowNr) {
    ui->initSlider(parentVar, "BPM", 60, 0, 255, false, [](JsonObject var) { //uiFun
      web->addResponse(var["id"], "comment", "in BPM!");
    });
    ui->initSlider(parentVar, "fade", 128);
    return true;
  }
};

class ConfettiEffect: public Effect {
public:
  const char * name() {
    return "Confetti 1D";
  }
  void loop(Leds &leds) {
    // random colored speckles that blink in and fade smoothly
    leds.fadeToBlackBy(10);
    int pos = random16(leds.nrOfLeds);
    leds[pos] += CHSV( gHue + random8(64), 200, 255);
  }
};

class BPMEffect: public Effect {
public:
  const char * name() {
    return "Beats per minute 1D";
  }

  void loop(Leds &leds) {
    // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
    uint8_t BeatsPerMinute = 62;
    uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
    for (uint16_t i = 0; i < leds.nrOfLeds; i++) { //9948
      leds[i] = ColorFromPalette(palette, gHue+(i*2), beat-gHue+(i*10));
    }
  }
  bool controls(JsonObject parentVar, uint8_t rowNr) {
    addPalette(parentVar, rowNr);
    return false;
  }
};

class JuggleEffect: public Effect {
public:
  const char * name() {
    return "Juggle 1D";
  }
  void loop(Leds &leds) {
    // eight colored dots, weaving in and out of sync with each other
    leds.fadeToBlackBy(20);
    uint8_t dothue = 0;
    for (uint8_t i = 0; i < 8; i++) {
      leds[beatsin16( i+7, 0, leds.nrOfLeds-1 )] |= CHSV(dothue, 200, 255);
      dothue += 32;
    }
  }
};

class Ripples3DEffect: public Effect {
public:
  const char * name() {
    return "Ripples 3D";
  }
  void loop(Leds &leds) {
    uint8_t interval = mdl->getValue("interval");

    float ripple_interval = 1.3 * (interval/128.0);

    leds.fill_solid(CRGB::Black);
    // fill(CRGB::Black);

    Coord3D pos = {0,0,0};
    for (pos.z=0; pos.z<leds.size.z; pos.z++) {
        for (pos.x=0; pos.x<leds.size.x; pos.x++) {
            float d = leds.fixture->distance(3.5, 3.5, 0, pos.x, pos.z, 0)/9.899495*leds.size.y;
            pos.y = floor(leds.size.y/2.0+sinf(d/ripple_interval + now/100/((256.0-128.0)/20.0))*leds.size.y/2.0); //between 0 and 8

            leds[pos] = CHSV( gHue + random8(64), 200, 255);// ColorFromPalette(pal,call, bri, LINEARBLEND);
        }
    }
  }
  bool controls(JsonObject parentVar, uint8_t rowNr) {
    ui->initSlider(parentVar, "interval", 128);
    return true;
  }
};

class SphereMove3DEffect: public Effect {
public:
  const char * name() {
    return "SphereMove 3D";
  }
  void loop(Leds &leds) {
    uint16_t origin_x, origin_y, origin_z, d;
    float diameter;

    leds.fill_solid(CRGB::Black);
    // fill(CRGB::Black);

    uint32_t interval = now/100/((256.0-128.0)/20.0);

    Coord3D origin;
    origin.x = 3.5+sinf(interval)*2.5;
    origin.y = 3.5+cosf(interval)*2.5;
    origin.z = 3.5+cosf(interval)*2.0;

    diameter = 2.0+sinf(interval/3.0);

    // CRGBPalette256 pal;
    Coord3D pos;
    for (pos.x=0; pos.x<leds.size.x; pos.x++) {
        for (pos.y=0; pos.y<leds.size.y; pos.y++) {
            for (pos.z=0; pos.z<leds.size.z; pos.z++) {
                d = leds.fixture->distance(pos.x, pos.y, pos.z, origin.x, origin.y, origin.z);

                if (d>diameter && d<diameter+1) {
                  leds[pos] = CHSV( gHue + random8(64), 200, 255);// ColorFromPalette(pal,call, bri, LINEARBLEND);
                }
            }
        }
    }
  }
}; // SphereMove3DEffect

//Frizzles2D inspired by WLED, Stepko, Andrew Tuline, https://editor.soulmatelights.com/gallery/640-color-frizzles
class Frizzles2D: public Effect {
public:
  const char * name() {
    return "Frizzles 2D";
  }

  void loop(Leds &leds) {
    leds.fadeToBlackBy(16);

    for (size_t i = 8; i > 0; i--) {
      Coord3D pos = {0,0,0};
      pos.x = beatsin8(mdl->getValue("BPM").as<int>()/8 + i, 0, leds.size.x - 1);
      pos.y = beatsin8(mdl->getValue("intensity").as<int>()/8 - i, 0, leds.size.y - 1);
      CRGB color = ColorFromPalette(palette, beatsin8(12, 0, 255), 255);
      leds[pos] = color;
    }
    leds.blur2d(mdl->getValue("blur"));
  }
  bool controls(JsonObject parentVar, uint8_t rowNr) {
    addPalette(parentVar, rowNr);
    ui->initSlider(parentVar, "BPM", 60);
    ui->initSlider(parentVar, "intensity", 128);
    ui->initSlider(parentVar, "blur", 128);
    return true;
  }
}; // Frizzles2D

class Lines2D: public Effect {
public:
  const char * name() {
    return "Lines 2D";
  }

  void loop(Leds &leds) {
    leds.fadeToBlackBy(100);

    Coord3D pos = {0,0,0};
    if (mdl->getValue("Vertical").as<bool>()) {
      pos.x = map(beat16( mdl->getValue("BPM").as<int>()), 0, uint16_t(-1), 0, leds.size.x-1 ); //instead of call%width

      for (pos.y = 0; pos.y <  leds.size.y; pos.y++) {
        leds[pos] = CHSV( gHue, 255, 192);
      }
    } else {
      pos.y = map(beat16( mdl->getValue("BPM").as<int>()), 0, uint16_t(-1), 0, leds.size.y-1 ); //instead of call%height
      for (pos.x = 0; pos.x <  leds.size.x; pos.x++) {
        leds[pos] = CHSV( gHue, 255, 192);
      }
    }
  }

  bool controls(JsonObject parentVar, uint8_t rowNr) {
    ui->initSlider(parentVar, "BPM", 60);
    ui->initCheckBox(parentVar, "Vertical");
    return true;
  }
}; // Lines2D

uint8_t gamma8(uint8_t b) { //we do nothing with gamma for now
  return b;
}

//DistortionWaves2D inspired by WLED, ldirko and blazoncek, https://editor.soulmatelights.com/gallery/1089-distorsion-waves
class DistortionWaves2D: public Effect {
public:
  const char * name() {
    return "DistortionWaves 2D";
  }

  void loop(Leds &leds) {

    uint8_t speed = mdl->getValue("speed").as<int>()/32;
    uint8_t scale = mdl->getValue("scale").as<int>()/32;

    uint8_t  w = 2;

    uint16_t a  = now/32;
    uint16_t a2 = a/2;
    uint16_t a3 = a/3;

    uint16_t cx =  beatsin8(10-speed,0,leds.size.x-1)*scale;
    uint16_t cy =  beatsin8(12-speed,0,leds.size.y-1)*scale;
    uint16_t cx1 = beatsin8(13-speed,0,leds.size.x-1)*scale;
    uint16_t cy1 = beatsin8(15-speed,0,leds.size.y-1)*scale;
    uint16_t cx2 = beatsin8(17-speed,0,leds.size.x-1)*scale;
    uint16_t cy2 = beatsin8(14-speed,0,leds.size.y-1)*scale;
    
    uint16_t xoffs = 0;
    Coord3D pos = {0,0,0};
    for (pos.x = 0; pos.x < leds.size.x; pos.x++) {
      xoffs += scale;
      uint16_t yoffs = 0;

      for (pos.y = 0; pos.y < leds.size.y; pos.y++) {
        yoffs += scale;

        byte rdistort = cos8((cos8(((pos.x<<3)+a )&255)+cos8(((pos.y<<3)-a2)&255)+a3   )&255)>>1; 
        byte gdistort = cos8((cos8(((pos.x<<3)-a2)&255)+cos8(((pos.y<<3)+a3)&255)+a+32 )&255)>>1; 
        byte bdistort = cos8((cos8(((pos.x<<3)+a3)&255)+cos8(((pos.y<<3)-a) &255)+a2+64)&255)>>1; 

        byte valueR = rdistort+ w*  (a- ( ((xoffs - cx)  * (xoffs - cx)  + (yoffs - cy)  * (yoffs - cy))>>7  ));
        byte valueG = gdistort+ w*  (a2-( ((xoffs - cx1) * (xoffs - cx1) + (yoffs - cy1) * (yoffs - cy1))>>7 ));
        byte valueB = bdistort+ w*  (a3-( ((xoffs - cx2) * (xoffs - cx2) + (yoffs - cy2) * (yoffs - cy2))>>7 ));

        valueR = gamma8(cos8(valueR));
        valueG = gamma8(cos8(valueG));
        valueB = gamma8(cos8(valueB));

        leds[pos] = CRGB(valueR, valueG, valueB);
      }
    }
  }
  bool controls(JsonObject parentVar, uint8_t rowNr) {
    ui->initSlider(parentVar, "speed", 128);
    ui->initSlider(parentVar, "scale", 128);
    return true;
  }
}; // DistortionWaves2D

//Octopus2D inspired by WLED, Stepko and Sutaburosu and blazoncek 
//Idea from https://www.youtube.com/watch?v=HsA-6KIbgto&ab_channel=GreatScott%21 (https://editor.soulmatelights.com/gallery/671-octopus)
class Octopus2D: public Effect {
public:
  const char * name() {
    return "Octopus 2D";
  }

  typedef struct {
    uint8_t angle;
    uint8_t radius;
  } map_t;

  void loop(Leds &leds) {

    const uint8_t mapp = 180 / max(leds.size.x,leds.size.y);

    uint8_t speed = mdl->getValue("speed");
    uint8_t offsetX = mdl->getValue("Offset X");
    uint8_t offsetY = mdl->getValue("Offset Y");
    uint8_t legs = mdl->getValue("Legs");

    sharedData.allocate(sizeof(map_t) * leds.size.x * leds.size.y + 2 * sizeof(uint8_t) + 2 * sizeof(uint16_t) + sizeof(uint32_t));
    map_t *rMap = sharedData.bind<map_t>(leds.size.x * leds.size.y); //array
    uint8_t *offsX = sharedData.bind<uint8_t>();
    uint8_t *offsY = sharedData.bind<uint8_t>();
    uint16_t *aux0 = sharedData.bind<uint16_t>();
    uint16_t *aux1 = sharedData.bind<uint16_t>();
    uint32_t *step = sharedData.bind<uint32_t>();
    if (!sharedData.allocated()) return;

    Coord3D pos = {0,0,0};

    // re-init if SEGMENT dimensions or offset changed
    if (*aux0 != leds.size.x || *aux1 != leds.size.y || offsetX != *offsX || offsetY != *offsY) {
      // *step = 0;
      *aux0 = leds.size.x;
      *aux1 = leds.size.y;
      *offsX = offsetX;
      *offsY = offsetY;
      const uint8_t C_X = leds.size.x / 2 + (offsetX - 128)*leds.size.x/255;
      const uint8_t C_Y = leds.size.y / 2 + (offsetY - 128)*leds.size.y/255;
      for (pos.x = 0; pos.x < leds.size.x; pos.x++) {
        for (pos.y = 0; pos.y < leds.size.y; pos.y++) {
          rMap[leds.XY(pos.x, pos.y)].angle = 40.7436f * atan2f(pos.y - C_Y, pos.x - C_X); // avoid 128*atan2()/PI
          rMap[leds.XY(pos.x, pos.y)].radius = hypotf(pos.x - C_X, pos.y - C_Y) * mapp; //thanks Sutaburosu
        }
      }
    }

    *step = now * speed / 32 / 10;//mdl->getValue("realFps").as<int>();  // WLEDMM 40fps

    for (pos.x = 0; pos.x < leds.size.x; pos.x++) {
      for (pos.y = 0; pos.y < leds.size.y; pos.y++) {
        byte angle = rMap[leds.XY(pos.x,pos.y)].angle;
        byte radius = rMap[leds.XY(pos.x,pos.y)].radius;
        //CRGB c = CHSV(SEGENV.step / 2 - radius, 255, sin8(sin8((angle * 4 - radius) / 4 + SEGENV.step) + radius - SEGENV.step * 2 + angle * (SEGMENT.custom3/3+1)));
        uint16_t intensity = sin8(sin8((angle * 4 - radius) / 4 + *step/2) + radius - *step + angle * legs);
        intensity = map(intensity*intensity, 0, 65535, 0, 255); // add a bit of non-linearity for cleaner display
        CRGB color = ColorFromPalette(palette, *step / 2 - radius, intensity);
        leds[pos] = color;
      }
    }
  }
  bool controls(JsonObject parentVar, uint8_t rowNr) {
    addPalette(parentVar, rowNr);
    ui->initSlider(parentVar, "speed", 128, 1, 255); //start with speed 1
    ui->initSlider(parentVar, "Offset X", 128);
    ui->initSlider(parentVar, "Offset Y", 128);
    ui->initSlider(parentVar, "Legs", 4, 1, 8);
    return true;
  }
}; // Octopus2D

//Lissajous2D inspired by WLED, Andrew Tuline 
class Lissajous2D: public Effect {
public:
  const char * name() {
    return "Lissajous 2D";
  }

  void loop(Leds &leds) {

    uint8_t freqX = mdl->getValue("X frequency");
    uint8_t fadeRate = mdl->getValue("Fade rate");
    uint8_t speed = mdl->getValue("Speed");
    bool smooth = mdl->getValue("Smooth");

    leds.fadeToBlackBy(fadeRate);

    uint_fast16_t phase = now * speed / 256;  // allow user to control rotation speed, speed between 0 and 255!

    Coord3D locn = {0,0,0};
    if (smooth) { // WLEDMM: this is the original "float" code featuring anti-aliasing
        int maxLoops = max(192, 4*(leds.size.x+leds.size.y));
        maxLoops = ((maxLoops / 128) +1) * 128; // make sure whe have half or full turns => multiples of 128
        for (int i=0; i < maxLoops; i ++) {
          locn.x = float(sin8(phase/2 + (i* freqX)/64)) / 255.0f;  // WLEDMM align speed with original effect
          locn.y = float(cos8(phase/2 + i*2)) / 255.0f;
          //SEGMENT.setPixelColorXY(xlocn, ylocn, SEGMENT.color_from_palette(strip.now/100+i, false, PALETTE_SOLID_WRAP, 0)); // draw pixel with anti-aliasing
          unsigned palIndex = (256*locn.y) + phase/2 + (i* freqX)/64;
          // SEGMENT.setPixelColorXY(xlocn, ylocn, SEGMENT.color_from_palette(palIndex, false, PALETTE_SOLID_WRAP, 0)); // draw pixel with anti-aliasing - color follows rotation
          leds[locn] = ColorFromPalette(palette, palIndex);
        }
    } else
    for (int i=0; i < 256; i ++) {
      //WLEDMM: stick to the original calculations of xlocn and ylocn
      locn.x = sin8(phase/2 + (i*freqX)/64);
      locn.y = cos8(phase/2 + i*2);
      locn.x = (leds.size.x < 2) ? 1 : (map(2*locn.x, 0,511, 0,2*(leds.size.x-1)) +1) /2;    // softhack007: "*2 +1" for proper rounding
      locn.y = (leds.size.y < 2) ? 1 : (map(2*locn.y, 0,511, 0,2*(leds.size.y-1)) +1) /2;    // "leds.size.y > 2" is needed to avoid div/0 in map()
      // SEGMENT.setPixelColorXY((uint8_t)xlocn, (uint8_t)ylocn, SEGMENT.color_from_palette(strip.now/100+i, false, PALETTE_SOLID_WRAP, 0));
      leds[locn] = ColorFromPalette(palette, now/100+i);
    }
  }
  bool controls(JsonObject parentVar, uint8_t rowNr) {
    addPalette(parentVar, rowNr);
    ui->initSlider(parentVar, "X frequency", 64);
    ui->initSlider(parentVar, "Fade rate", 128);
    ui->initSlider(parentVar, "Speed", 128);
    ui->initCheckBox(parentVar, "Smooth");
    return true;
  }
}; // Lissajous2D


#define maxNumBalls 16

class BouncingBalls1D: public Effect {
public:
  //BouncingBalls1D  inspired by WLED
  //each needs 12 bytes
  typedef struct Ball {
    unsigned long lastBounceTime;
    float impactVelocity;
    float height;
  } ball;

  const char * name() {
    return "Bouncing Balls 1D";
  }

  void loop(Leds &leds) {
    uint8_t grav = mdl->getValue("gravity");
    uint8_t numBalls = mdl->getValue("balls");

    sharedData.allocate(sizeof(Ball) * maxNumBalls);
    Ball *balls = sharedData.bind<Ball>(maxNumBalls); //array
    if (!sharedData.allocated()) return;

    leds.fill_solid(CRGB::Black);

    // non-chosen color is a random color
    const float gravity = -9.81f; // standard value of gravity
    // const bool hasCol2 = SEGCOLOR(2);
    const unsigned long time = now;

    //not necessary as sharedData is cleared at setup(Leds &leds)
    // if (call == 0) {
    //   for (size_t i = 0; i < maxNumBalls; i++) balls[i].lastBounceTime = time;
    // }

    for (size_t i = 0; i < numBalls; i++) {
      float timeSinceLastBounce = (time - balls[i].lastBounceTime)/((255-grav)/64 +1);
      float timeSec = timeSinceLastBounce/1000.0f;
      balls[i].height = (0.5f * gravity * timeSec + balls[i].impactVelocity) * timeSec; // avoid use pow(x, 2) - its extremely slow !

      if (balls[i].height <= 0.0f) {
        balls[i].height = 0.0f;
        //damping for better effect using multiple balls
        float dampening = 0.9f - float(i)/float(numBalls * numBalls); // avoid use pow(x, 2) - its extremely slow !
        balls[i].impactVelocity = dampening * balls[i].impactVelocity;
        balls[i].lastBounceTime = time;

        if (balls[i].impactVelocity < 0.015f) {
          float impactVelocityStart = sqrtf(-2.0f * gravity) * random8(5,11)/10.0f; // randomize impact velocity
          balls[i].impactVelocity = impactVelocityStart;
        }
      } else if (balls[i].height > 1.0f) {
        continue; // do not draw OOB ball
      }

      // uint32_t color = SEGCOLOR(0);
      // if (SEGMENT.palette) {
      //   color = SEGMENT.color_wheel(i*(256/MAX(numBalls, 8)));
      // } 
      // else if (hasCol2) {
      //   color = SEGCOLOR(i % NUM_COLORS);
      // }

      int pos = roundf(balls[i].height * (leds.nrOfLeds - 1));

      CRGB color = ColorFromPalette(palette, i*(256/max(numBalls, (uint8_t)8)), 255);

      leds[pos] = color;
      // if (SEGLEN<32) SEGMENT.setPixelColor(indexToVStrip(pos, stripNr), color); // encode virtual strip into index
      // else           SEGMENT.setPixelColor(balls[i].height + (stripNr+1)*10.0f, color);
    } //balls
  }

  bool controls(JsonObject parentVar, uint8_t rowNr) {
    addPalette(parentVar, rowNr);
    ui->initSlider(parentVar, "gravity", 128);
    ui->initSlider(parentVar, "balls", 8, 1, 16);
    return true;
  }
}; // BouncingBalls2D

class RingEffect:public Effect {
  protected:

    void setRing(Leds &leds, int ring, CRGB colour) { //so britisch ;-)
      leds[ring] = colour;
    }

};

class RingRandomFlow:public RingEffect {
public:
  const char * name() {
    return "RingRandomFlow 1D";
  }

  void loop(Leds &leds) {
    sharedData.allocate(sizeof(uint8_t) * leds.nrOfLeds);
    uint8_t *hue = sharedData.bind<uint8_t>(leds.nrOfLeds); //array
    if (!sharedData.allocated()) return;

    hue[0] = random(0, 255);
    for (int r = 0; r < leds.nrOfLeds; r++) {
      setRing(leds, r, CHSV(hue[r], 255, 255));
    }
    for (int r = (leds.nrOfLeds - 1); r >= 1; r--) {
      hue[r] = hue[(r - 1)]; // set this ruing based on the inner
    }
    // FastLED.delay(SPEED);
  }
};


#ifdef USERMOD_WLEDAUDIO

class GEQEffect:public Effect {
public:
  const char * name() {
    return "GEQ 2D";
  }

  void setup(Leds &leds) {
    leds.fadeToBlackBy(16);
  }

  void loop(Leds &leds) {
    sharedData.allocate(sizeof(uint16_t) * leds.size.x + sizeof(uint32_t));
    uint16_t *previousBarHeight = sharedData.bind<uint16_t>(leds.size.x); //array
    uint32_t *step = sharedData.bind<uint32_t>();
    if (!sharedData.allocated()) return;

    const int NUM_BANDS = NUM_GEQ_CHANNELS ; // map(SEGMENT.custom1, 0, 255, 1, 16);

    #ifdef SR_DEBUG
    uint8_t samplePeak = *(uint8_t*)um_data->u_data[3];
    #endif

    uint8_t fadeOut = mdl->getValue("fadeOut");
    uint8_t ripple = mdl->getValue("ripple"); 
    bool colorBars = mdl->getValue("colorBars");
    bool smoothBars = mdl->getValue("smoothBars");

    bool rippleTime = false;
    if (now - *step >= (256U - ripple)) {
      *step = now;
      rippleTime = true;
    }

    int fadeoutDelay = (256 - fadeOut) / 64; //256..1 -> 4..0
    size_t beat = map(beat16( fadeOut), 0, uint16_t(-1), 0, fadeoutDelay-1 ); // instead of call%fadeOutDelay

    if ((fadeoutDelay <= 1 ) || (beat == 0)) leds.fadeToBlackBy(fadeOut);

    uint16_t lastBandHeight = 0;  // WLEDMM: for smoothing out bars

    //WLEDMM: evenly ditribut bands
    float bandwidth = (float)leds.size.x / NUM_BANDS;
    float remaining = bandwidth;
    uint8_t band = 0;
    Coord3D pos = {0,0,0};
    for (pos.x=0; pos.x < leds.size.x; pos.x++) {
      //WLEDMM if not enough remaining
      if (remaining < 1) {band++; remaining+= bandwidth;} //increase remaining but keep the current remaining
      remaining--; //consume remaining

      // USER_PRINTF("x %d b %d n %d w %f %f\n", x, band, NUM_BANDS, bandwidth, remaining);
      uint8_t frBand = ((NUM_BANDS < 16) && (NUM_BANDS > 1)) ? map(band, 0, NUM_BANDS - 1, 0, 15):band; // always use full range. comment out this line to get the previous behaviour.
      // frBand = constrain(frBand, 0, 15); //WLEDMM can never be out of bounds (I think...)
      uint16_t colorIndex = frBand * 17; //WLEDMM 0.255
      uint16_t bandHeight = wledAudioMod->fftResults[frBand];  // WLEDMM we use the original ffResult, to preserve accuracy

      // WLEDMM begin - smooth out bars
      if ((pos.x > 0) && (pos.x < (leds.size.x-1)) && (smoothBars)) {
        // get height of next (right side) bar
        uint8_t nextband = (remaining < 1)? band +1: band;
        nextband = constrain(nextband, 0, 15);  // just to be sure
        frBand = ((NUM_BANDS < 16) && (NUM_BANDS > 1)) ? map(nextband, 0, NUM_BANDS - 1, 0, 15):nextband; // always use full range. comment out this line to get the previous behaviour.
        uint16_t nextBandHeight = wledAudioMod->fftResults[frBand];
        // smooth Band height
        bandHeight = (7*bandHeight + 3*lastBandHeight + 3*nextBandHeight) / 12;   // yeees, its 12 not 13 (10% amplification)
        bandHeight = constrain(bandHeight, 0, 255);   // remove potential over/underflows
        colorIndex = map(pos.x, 0, leds.size.x-1, 0, 255); //WLEDMM
      }
      lastBandHeight = bandHeight; // remember BandHeight (left side) for next iteration
      uint16_t barHeight = map(bandHeight, 0, 255, 0, leds.size.y); // Now we map bandHeight to barHeight. do not subtract -1 from leds.size.y here
      // WLEDMM end

      if (barHeight > leds.size.y) barHeight = leds.size.y;                      // WLEDMM map() can "overshoot" due to rounding errors
      if (barHeight > previousBarHeight[pos.x]) previousBarHeight[pos.x] = barHeight; //drive the peak up

      CRGB ledColor = CRGB::Black;

      for (pos.y=0; pos.y < barHeight; pos.y++) {
        if (colorBars) //color_vertical / color bars toggle
          colorIndex = map(pos.y, 0, leds.size.y-1, 0, 255);

        ledColor = ColorFromPalette(palette, (uint8_t)colorIndex);

        leds.setPixelColor(leds.XY(pos.x, leds.size.y - 1 - pos.y), ledColor);
      }

      if ((ripple > 0) && (previousBarHeight[pos.x] > 0) && (previousBarHeight[pos.x] < leds.size.y))  // WLEDMM avoid "overshooting" into other segments
        leds.setPixelColor(leds.XY(pos.x, leds.size.y - previousBarHeight[pos.x]), CHSV( gHue, 255, 192)); // take gHue color for the time being

      if (rippleTime && previousBarHeight[pos.x]>0) previousBarHeight[pos.x]--;    //delay/ripple effect

    }
  }

  bool controls(JsonObject parentVar, uint8_t rowNr) {
    addPalette(parentVar, rowNr);
    ui->initSlider(parentVar, "fadeOut", 255);
    ui->initSlider(parentVar, "ripple", 128);
    ui->initCheckBox(parentVar, "colorBars");
    ui->initCheckBox(parentVar, "smoothBars", true); //default on

    // Nice an effect can register it's own DMX channel, but not a fan of repeating the range and type of the param

    #ifdef USERMOD_E131

      if (e131mod->isEnabled) {
        e131mod->patchChannel(3, "fadeOut", 255); // TODO: add constant for name
        e131mod->patchChannel(4, "ripple", 255);
        ui->processUiFun("e131Tbl");
      }

    #endif

    return true;
  }
};

class AudioRings:public RingEffect {
public:
  const char * name() {
    return "AudioRings 1D";
  }

  void loop(Leds &leds) {
    for (int i = 0; i < 7; i++) { // 7 rings

      uint8_t val;
      if(mdl->getValue("inWards").as<bool>()) {
        val = wledAudioMod->fftResults[(i*2)];
      }
      else {
        int b = 14 -(i*2);
        val = wledAudioMod->fftResults[b];
      }
  
      // Visualize leds to the beat
      CRGB color = ColorFromPalette(palette, val, val);
//      CRGB color = ColorFromPalette(currentPalette, val, 255, currentBlending);
//      color.nscale8_video(val);
      setRing(leds, i, color);
//        setRingFromFtt((i * 2), i); 
    }

    setRingFromFtt(leds, 2, 7); // set outer ring to bass
    setRingFromFtt(leds, 0, 8); // set outer ring to bass

  }
  void setRingFromFtt(Leds &leds, int index, int ring) {
    uint8_t val = wledAudioMod->fftResults[index];
    // Visualize leds to the beat
    CRGB color = ColorFromPalette(palette, val, 255);
    color.nscale8_video(val);
    setRing(leds, ring, color);
  }

  bool controls(JsonObject parentVar, uint8_t rowNr) {
    addPalette(parentVar, rowNr);
    ui->initCheckBox(parentVar, "inWards");
    return true;
  }
};

class FreqMatrix:public Effect {
public:
  const char * name() {
    return "FreqMatrix 1D";
  }

  void setup(Leds &leds) {
    leds.fadeToBlackBy(16);
  }

  void loop(Leds &leds) {
    sharedData.allocate(sizeof(uint8_t));
    uint8_t *aux0 = sharedData.bind<uint8_t>();
    if (!sharedData.allocated()) return;

    uint8_t speed = mdl->getValue("Speed");
    uint8_t fx = mdl->getValue("Sound effect");
    uint8_t lowBin = mdl->getValue("Low bin");
    uint8_t highBin = mdl->getValue("High bin");
    uint8_t sensitivity10 = mdl->getValue("Sensivity");

    uint8_t secondHand = (speed < 255) ? (micros()/(256-speed)/500 % 16) : 0;
    if((speed > 254) || (*aux0 != secondHand)) {   // WLEDMM allow run run at full speed
      *aux0 = secondHand;

      // Pixel brightness (value) based on volume * sensitivity * intensity
      // uint_fast8_t sensitivity10 = map(sensitivity, 0, 31, 10, 100); // reduced resolution slider // WLEDMM sensitivity * 10, to avoid losing precision
      int pixVal = wledAudioMod->sync.volumeSmth * (float)fx * (float)sensitivity10 / 2560.0f; // WLEDMM 2560 due to sensitivity * 10
      if (pixVal > 255) pixVal = 255;  // make a brightness from the last avg

      CRGB color = CRGB::Black;

      if (wledAudioMod->sync.FFT_MajorPeak > MAX_FREQUENCY) wledAudioMod->sync.FFT_MajorPeak = 1;
      // MajorPeak holds the freq. value which is most abundant in the last sample.
      // With our sampling rate of 10240Hz we have a usable freq range from roughtly 80Hz to 10240/2 Hz
      // we will treat everything with less than 65Hz as 0

      if ((wledAudioMod->sync.FFT_MajorPeak > 80.0f) && (wledAudioMod->sync.volumeSmth > 0.25f)) { // WLEDMM
        // Pixel color (hue) based on major frequency
        int upperLimit = 80 + 42 * highBin;
        int lowerLimit = 80 + 3 * lowBin;
        //uint8_t i =  lowerLimit!=upperLimit ? map(FFT_MajorPeak, lowerLimit, upperLimit, 0, 255) : FFT_MajorPeak;  // (original formula) may under/overflow - so we enforce uint8_t
        int freqMapped =  lowerLimit!=upperLimit ? map(wledAudioMod->sync.FFT_MajorPeak, lowerLimit, upperLimit, 0, 255) : wledAudioMod->sync.FFT_MajorPeak;  // WLEDMM preserve overflows
        uint8_t i = abs(freqMapped) & 0xFF;  // WLEDMM we embrace overflow ;-) by "modulo 256"

        color = CHSV(i, 240, (uint8_t)pixVal); // implicit conversion to RGB supplied by FastLED
      }

      // shift the pixels one pixel up
      leds.setPixelColor(0, color);
      for (int i = leds.nrOfLeds - 1; i > 0; i--) leds.setPixelColor(i, leds.getPixelColor(i-1));
    }
  }

  bool controls(JsonObject parentVar, uint8_t rowNr) {
    ui->initSlider(parentVar, "Speed", 255);
    ui->initSlider(parentVar, "Sound effect", 128);
    ui->initSlider(parentVar, "Low bin", 18);
    ui->initSlider(parentVar, "High bin", 48);
    ui->initSlider(parentVar, "Sensivity", 30, 10, 100);

    return true;
  }
};

#endif // End Audio Effects

class Effects {
public:
  std::vector<Effect *> effects;

  Effects() {
    //create effects before fx.chFun is called
    effects.push_back(new SolidEffect);
    effects.push_back(new RainbowEffect);
    effects.push_back(new RainbowWithGlitterEffect);
    effects.push_back(new SinelonEffect);
    effects.push_back(new RunningEffect);
    effects.push_back(new ConfettiEffect);
    effects.push_back(new BPMEffect);
    effects.push_back(new JuggleEffect);
    effects.push_back(new Ripples3DEffect);
    effects.push_back(new SphereMove3DEffect);
    effects.push_back(new Frizzles2D);
    effects.push_back(new Lines2D);
    effects.push_back(new DistortionWaves2D);
    effects.push_back(new Octopus2D);
    effects.push_back(new Lissajous2D);
    effects.push_back(new BouncingBalls1D);
    effects.push_back(new RingRandomFlow);
    #ifdef USERMOD_WLEDAUDIO
      effects.push_back(new GEQEffect);
      effects.push_back(new AudioRings);
      effects.push_back(new FreqMatrix);
    #endif
  }

  void setup() {
    //check of no local variables (should be only 4 bytes): tbd: can we loop over effects (sizeof(effect does not work))
    // for (Effect *effect:effects) {
    //     USER_PRINTF("Size of %s is %d\n", effect->name(), sizeof(*effect));
    // }
    // USER_PRINTF("Size of %s is %d\n", "RainbowEffect", sizeof(RainbowEffect));
    // USER_PRINTF("Size of %s is %d\n", "RainbowWithGlitterEffect", sizeof(RainbowWithGlitterEffect));
    // USER_PRINTF("Size of %s is %d\n", "SinelonEffect", sizeof(SinelonEffect));
    // USER_PRINTF("Size of %s is %d\n", "RunningEffect", sizeof(RunningEffect));
    // USER_PRINTF("Size of %s is %d\n", "ConfettiEffect", sizeof(ConfettiEffect));
    // USER_PRINTF("Size of %s is %d\n", "BPMEffect", sizeof(BPMEffect));
    // USER_PRINTF("Size of %s is %d\n", "JuggleEffect", sizeof(JuggleEffect));
    // USER_PRINTF("Size of %s is %d\n", "Ripples3DEffect", sizeof(Ripples3DEffect));
    // USER_PRINTF("Size of %s is %d\n", "SphereMove3DEffect", sizeof(SphereMove3DEffect));
    // USER_PRINTF("Size of %s is %d\n", "Frizzles2D", sizeof(Frizzles2D));
    // USER_PRINTF("Size of %s is %d\n", "Lines2D", sizeof(Lines2D));
    // USER_PRINTF("Size of %s is %d\n", "DistortionWaves2D", sizeof(DistortionWaves2D));
    // USER_PRINTF("Size of %s is %d\n", "Octopus2D", sizeof(Octopus2D));
    // USER_PRINTF("Size of %s is %d\n", "Lissajous2D", sizeof(Lissajous2D));
    // USER_PRINTF("Size of %s is %d\n", "BouncingBalls1D", sizeof(BouncingBalls1D));
    // USER_PRINTF("Size of %s is %d\n", "RingRandomFlow", sizeof(RingRandomFlow));
    // #ifdef USERMOD_WLEDAUDIO
    //   USER_PRINTF("Size of %s is %d\n", "GEQEffect", sizeof(GEQEffect));
    //   USER_PRINTF("Size of %s is %d\n", "AudioRings", sizeof(AudioRings));
    // #endif
  }

  void loop(Leds &leds) {
    now = millis(); //tbd timebase

    effects[leds.fx%effects.size()]->loop(leds);

    #ifdef USERMOD_WLEDAUDIO

      if (mdl->getValue("mHead") ) {
        leds.fixture->head.x = wledAudioMod->fftResults[3];
        leds.fixture->head.y = wledAudioMod->fftResults[8];
        leds.fixture->head.z = wledAudioMod->fftResults[13];
      }

    #endif

    call++;

    EVERY_N_MILLISECONDS( 20 ) { gHue++; } // slowly cycle the "base color" through the rainbow
  }

  bool setEffect(Leds &leds, JsonObject var, uint8_t rowNr) {
    bool doMap = false;

    leds.fx = mdl->getValue(var, rowNr);

    if (rowNr != UINT8_MAX)
      var["rowNr"] = rowNr; //store the rownNr of the updated value to send back to ui
    else 
      var.remove("rowNr");

    USER_PRINTF("setEffect %d\n", leds.fx);

    if (leds.fx < effects.size()) {

      //tbd: make property of effects
      if (strstr(effects[leds.fx]->name(), "2D")) {
        if (leds.effectDimension != 2) {
          leds.effectDimension = 2;
          doMap = true;
        }
      }
      else if (strstr(effects[leds.fx]->name(), "3D")) {
        if (leds.effectDimension != 3) {
          leds.effectDimension = 3;
          doMap = true;
        }
      }
      else {
        if (leds.effectDimension != 1) {
          leds.effectDimension = 1;
          doMap = true;
        }
      }

      sharedData.clear(); //make sure all values are 0

      // nullify values for this row
      if (rowNr != UINT8_MAX) {
        for (JsonObject var: var["n"].as<JsonArray>()) {
          if (var["value"].is<JsonArray>()) {
            var["value"][rowNr] = -99;
          } else {
            var["value"].to<JsonArray>();
            var["value"][rowNr] = -99; //unused value for this row, so don't show
          }
          // mdl->setValue(var, -99, rowNr); //unused value for this row, so don't show
        }
        // set all values null for this row

        // var.remove("value");
      }
      else 
        var.remove("n"); //tbd: we should also remove the uiFun and chFun !!
        //tbd: we need to reuse the values set...

      // // nullify values for this row
      // for (JsonObject var: var["n"].as<JsonArray>()) {
      //   if (rowNr != UINT8_MAX) {
      //     if (var["value"].is<JsonArray>()) {
      //       var["value"][rowNr] = -99; //unused value for this row, so don't show
      //     } else {
      //       var["value"].to<JsonArray>();
      //       var["value"][rowNr] = -99; //unused value for this row, so don't show
      //     }
      //   }
      //   // else 
      //     // var["value"] = -99;
      // }
      // // else 
      // //   var.remove("n"); //tbd: we should also remove the uiFun and chFun !!

      Effect* effect = effects[leds.fx];
      effect->controls(var, rowNr);

      effect->setup(leds); //if changed then run setup once (like call==0 in WLED)

      JsonDocument *responseDoc = web->getResponseDoc();
      responseDoc->clear(); //needed for deserializeJson?
      JsonObject responseObject = responseDoc->to<JsonObject>();

      responseObject["details"] = var;

      print->printJson("var", responseObject);
      web->sendDataWs(responseObject); //always send, also when no children, to remove them from ui

    } // fx < size

    return doMap;
  }

};