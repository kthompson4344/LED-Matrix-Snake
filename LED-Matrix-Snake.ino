#include <SmartMatrix3.h>

/****************LED MATRIX CONSTANTS***************/
#define COLOR_DEPTH 24                  // known working: 24, 48 - If the sketch uses type `rgb24` directly, COLOR_DEPTH must be 24
const uint8_t kMatrixWidth = 32;        // known working: 32, 64, 96, 128
const uint8_t kMatrixHeight = 32;       // known working: 16, 32, 48, 64
const uint8_t kRefreshDepth = 36;       // known working: 24, 36, 48
const uint8_t kDmaBufferRows = 4;       // known working: 2-4, use 2 to save memory, more to keep from dropping frames and automatically lowering refresh rate
const uint8_t kPanelType = SMARTMATRIX_HUB75_32ROW_MOD16SCAN;   // use SMARTMATRIX_HUB75_16ROW_MOD8SCAN for common 16x32 panels
const uint8_t kMatrixOptions = (SMARTMATRIX_OPTIONS_NONE);      // see http://docs.pixelmatix.com/SmartMatrix for options
const uint8_t kBackgroundLayerOptions = (SM_BACKGROUND_OPTIONS_NONE);
const uint8_t kScrollingLayerOptions = (SM_SCROLLING_OPTIONS_NONE);
const uint8_t kIndexedLayerOptions = (SM_INDEXED_OPTIONS_NONE);

SMARTMATRIX_ALLOCATE_BUFFERS(matrix, kMatrixWidth, kMatrixHeight, kRefreshDepth, kDmaBufferRows, kPanelType, kMatrixOptions);
SMARTMATRIX_ALLOCATE_BACKGROUND_LAYER(backgroundLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kBackgroundLayerOptions);
SMARTMATRIX_ALLOCATE_SCROLLING_LAYER(scrollingLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kScrollingLayerOptions);
SMARTMATRIX_ALLOCATE_INDEXED_LAYER(indexedLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kIndexedLayerOptions);

const int defaultBrightness = 100 * (255 / 100); // full brightness
const int defaultScrollOffset = 6;
const rgb24 defaultBackgroundColor = {0, 0, 0};
const int width = kMatrixWidth;
const int height = kMatrixHeight;

/****************SNAKE CONSTANTS***************/
const int lengthAddition = 4;
const rgb24 snakeColor = {0x0, 0xff, 0xff};
const rgb24 foodColor = {0xff, 0, 0};
const float snakeSpeed = 0.1; //seconds

int head[2] = {width / 2, height / 2};
int food[2] = {0};
int snakeScore = 0;
int scoreIncrement = 10;
int snakeGame[height][width] = {{0}};
int snakeLength = 1;
IntervalTimer snakeTimer;
int direction = 4; //0=N, 1=E, 2=S, 3=W
bool readKey = false;
bool readPosition = false;
bool notMoving = true;
bool gameOver = false;

void setup() {
  Serial.begin(115200);

  matrix.addLayer(&backgroundLayer);
  matrix.addLayer(&scrollingLayer);
  matrix.addLayer(&indexedLayer);
  matrix.begin();
  matrix.setBrightness(defaultBrightness);
  scrollingLayer.setOffsetFromTop(defaultScrollOffset);
  backgroundLayer.enableColorCorrection(true);

  randomSeed(analogRead(A20));//Read noise to seed RNG

  newFood();
  backgroundLayer.drawPixel(head[0], head[1], snakeColor);
  backgroundLayer.swapBuffers();
  snakeTimer.begin(updateHead, snakeSpeed * 1000000);
}

void loop() {
  keyPressSerial(readKey);
  updatePosition(readPosition);
  endGame(gameOver);
}

void keyPressSerial(bool updateKey) {
  if (updateKey) {
    if (Serial.available() > 0) {
      notMoving = false;
      char key = Serial.read();
      if (key == 'w') {
        direction = 0;
      }
      else if (key == 'd') {
        direction = 1;
      }
      else if (key == 's') {
        direction = 2;
      }
      else if (key == 'a') {
        direction = 3;
      }
    }
    readKey = false;
  }
}

void updateHead() {
  readKey = true;
  readPosition = true;
  if (direction == 0) {
    head[1]--;
  }
  else if (direction == 1) {
    head[0]++;
  }
  else if (direction == 2) {
    head[1]++;
  }
  else if (direction == 3) {
    head[0]--;
  }
}

void updatePosition(bool update) {
  /* Snake play field is held in snakeGame 2D array. Vales are as follows:
   *  0: empty
   *  -1: food
   *  1: snake head
   *  > 1: snake tail
   *
   *  example snake:
   *  ------moving to the right ---->
   *  3  2  1  []
   *  4  3  2  1
   *     4  3  2  1
   *        4  3  2  1
   */
  if (update) {
    // used to prevent blinking of snake at the beginning
    if (notMoving == false) {
      // refresh field
      backgroundLayer.fillScreen({0, 0, 0});

      if (food[0] == head[0] && food[1] == head[1]) { // if snake is eating food
        //add an amount to snake length
        snakeLength += lengthAddition;
        snakeScore += scoreIncrement;
        snakeGame[food[1]][food[0]] = 0;
        //make new food location, set to -1 on field, draw
        newFood();
      }
      else {
        // keep drawing food
        backgroundLayer.drawPixel(food[0], food[1], foodColor);
      }

      // search field for snake parts, update locations, draw new locations
      for (int i = 0; i < width; i++) {
        for (int j = 0; j < height; j++) {
          if (snakeGame[j][i] < snakeLength && snakeGame[j][i] >= 1) {
            backgroundLayer.drawPixel(i, j, snakeColor);
            snakeGame[j][i]++;
          }
          else if (snakeGame[j][i] == snakeLength) {// removes end of tail
            snakeGame[j][i] = 0;
          }
        }
      }
      //check for collision with wall or body
      Serial.println(snakeGame[head[1]][head[0]]);
      if (snakeGame[head[1]][head[0]] > 0 || head[0] >= width || head[0] < 0 || head[1] >= height || head[1] < 0) {
        gameOver = true;
      }
      // snake head is 1
      snakeGame[head[1]][head[0]] = 1;
      // draw head
      backgroundLayer.drawPixel(head[0], head[1], snakeColor);
      // push set pixels to the screen
      backgroundLayer.swapBuffers();
    }
    readPosition = false;
    update = false;
  }
}

void newFood() {
  //make new food location, make sure it isn't where the snake is, set to -1 on field, draw
  bool isGood = false;
  while (isGood == false) {
    food[0] = random(0, width);
    food[1] = random(0, height);
    if (snakeGame[food[1]][food[0]] == 0) {
      isGood = true;
    }
    else {
      isGood = false;
    }
  }
  snakeGame[food[1]][food[0]] = -1;
  backgroundLayer.drawPixel(food[0], food[1], foodColor);
}

void endGame(bool isGameOver) {
  if (isGameOver) {
    char stringSnakeScore[10];
    snakeTimer.end();
    readKey = false;
    readPosition = false;
    notMoving = true;
    backgroundLayer.fillScreen({0, 0, 0});
    itoa(snakeScore, stringSnakeScore, 10);
//    scrollingLayer.setMode(stopped);
//    scrollingLayer.start("100", 1);
//    scrollingLayer.setStartOffsetFromLeft(5);
//    scrollingLayer.setFont(font5x7);
    scrollingLayer.setColor({0xff, 0xff, 0xff});
        backgroundLayer.setFont(font5x7);
//        scrollingLayer.setSpeed(40);
        backgroundLayer.drawString(2, int(height/4), {0xf0, 0xf0, 0xf0}, "Score:");
        backgroundLayer.drawString(2, matrix.getScreenHeight() / 2, {0xf0, 0xf0, 0xf0}, stringSnakeScore);
        backgroundLayer.swapBuffers();
  }
}

