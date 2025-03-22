// Librerías necesarias para el display OLED
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>  // Librería para almacenamiento persistente

// Pines del encoder
#define ENCODER_CLK  39    // Pin para CLK (A)
#define ENCODER_DT   38    // Pin para DT (B)
#define ENCODER_SW   37    // Pin para el botón del encoder
#define CONFIG_BUTTON 36   // pin para abrir la configuracion del encoder

// Configuración del OLED
#define SCREEN_WIDTH 128   // Ancho de la pantalla OLED en píxeles
#define SCREEN_HEIGHT 64   // Altura de la pantalla OLED en píxeles
#define OLED_RESET    -1   // Reset (-1 si no se usa)
#define OLED_ADDR     0x3C // Dirección I2C del OLED (0x3C para la mayoría de pantallas)
#define OLED_SDA      41   // GPIO para SDA
#define OLED_SCL      40   // GPIO para SCL

// Configuración del LED
#define LED_PIN 5          // Pin donde conectamos el LED

// Objeto OLED
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Variables para el encoder
volatile long encoderCount = 0;    // Contador de pasos del encoder
int lastStateCLK;                  // Último estado conocido del pin CLK
float mmPerStep = 0.5;             // Distancia en mm por paso del encoder (ahora variable)

// Distancia objetivo para encender el LED (valor inicial, modificable)
float TARGET_DISTANCE = 50.0;      // Se podrá ajustar dinámicamente

// Tiempo para evitar rebote del botón
unsigned long lastButtonPress = 0;
const int debounceTime = 200;      // Tiempo para evitar rebote en ms

// Variables para el menú de configuración
enum MenuState {
  NORMAL_MODE,
  CONFIG_MENU,
  ADJUST_MM_PER_STEP,
  ADJUST_TARGET_DISTANCE
};
MenuState currentState = NORMAL_MODE;
int menuSelection = 0;
const int MAX_MENU_ITEMS = 2;

// Objeto para guardar datos en memoria flash
Preferences preferences;

void setup() {
  Wire.begin(OLED_SDA, OLED_SCL);  // Inicializar I2C

  // Configuración de pines
  pinMode(ENCODER_CLK, INPUT);
  pinMode(ENCODER_DT, INPUT);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  pinMode(CONFIG_BUTTON, INPUT_PULLUP); // Botón de configuración con pull-up
  pinMode(LED_PIN, OUTPUT);             // Pin del LED como salida
  digitalWrite(LED_PIN, LOW);           // Apagar LED al inicio

  // Inicializar comunicación serial para depuración
  Serial.begin(115200);
  Serial.println("==============================================");
  Serial.println("  INICIANDO SISTEMA DE ENCODER CON DISPLAY   ");
  Serial.println("==============================================");

  // Inicializar almacenamiento Preferences
  preferences.begin("encoder", false);  // Espacio de nombres: "encoder"

  // Leer valores almacenados desde Preferences
  loadPreferences();

  // Inicializar OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("❌ ERROR: No se pudo iniciar el display OLED"));
    for (;;);
  } else {
    Serial.println(F("✅ Display OLED iniciado correctamente"));
  }

  // Pantalla de bienvenida
  displayWelcomeMessage();

  // Leer el estado inicial del CLK
  lastStateCLK = digitalRead(ENCODER_CLK);
  
  Serial.println("\n📊 SISTEMA LISTO - MODO NORMAL ACTIVO");
  Serial.println("----------------------------------------------");
}

// Mostrar mensaje de bienvenida al iniciar
void displayWelcomeMessage() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(10, 20);
  display.println("Encoder Iniciado");
  display.display();
  
  Serial.println("\n🔄 PANTALLA DE BIENVENIDA");
  Serial.println("Encoder Iniciado");
  
  delay(1000);
}

// Mostrar distancia actual en la pantalla OLED
void displayDistance(float distance) {
  display.clearDisplay();
  display.setTextSize(2);  // Tamaño de texto grande
  display.setTextColor(WHITE);
  display.setCursor(10, 10);
  display.print("Distancia:");
  display.setCursor(10, 30);
  display.print(distance, 2);  // Mostrar 2 decimales
  display.print(" mm");
  
  // Mostrar estado del LED
  display.setCursor(10, 50);
  display.setTextSize(1);
  if (distance >= TARGET_DISTANCE) {
    display.print("SALIDA: ON");
  } else {
    display.print("SALUDA: OFF");
  }
  
  display.display();
  
  // Mostrar la misma información en el monitor serial
  Serial.println("\n📏 MODO NORMAL - MEDICIÓN DE DISTANCIA");
  Serial.print("Distancia actual: ");
  Serial.print(distance, 2);
  Serial.println(" mm");
  Serial.print("Estado de SALIDA: ");
  Serial.println(distance >= TARGET_DISTANCE ? "ENCENDIDO ●" : "APAGADO ○");
  Serial.print("Distancia objetivo: ");
  Serial.print(TARGET_DISTANCE, 1);
  Serial.println(" mm");
  Serial.print("mm por paso: ");
  Serial.print(mmPerStep, 3);
  Serial.println(" mm");
}

// Mostrar menú de configuración
void displayConfigMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(5, 5);
  display.println("MENU DE CONFIGURACION");
  display.drawLine(0, 15, SCREEN_WIDTH, 15, WHITE);
  
  // Opción 1: Ajustar mm por paso
  display.setCursor(10, 25);
  if (menuSelection == 0) {
    display.print("> ");
  } else {
    display.print("  ");
  }
  display.print("Ajustar mm/paso");
  
  // Opción 2: Ajustar distancia objetivo
  display.setCursor(10, 40);
  if (menuSelection == 1) {
    display.print("> ");
  } else {
    display.print("  ");
  }
  display.print("Ajustar dist. obj.");
  
  // Instrucciones
  display.setCursor(5, 55);
  display.print("Pulsa para seleccionar");
  
  display.display();
  
  // Mostrar la misma información en el monitor serial
  Serial.println("\n⚙️ MENÚ DE CONFIGURACIÓN");
  Serial.println("----------------------------------------------");
  Serial.println(menuSelection == 0 ? "→ [Ajustar mm/paso]" : "  Ajustar mm/paso");
  Serial.println(menuSelection == 1 ? "→ [Ajustar dist. objetivo]" : "  Ajustar dist. objetivo");
  Serial.println("----------------------------------------------");
  Serial.println("Usar encoder para navegar, botón para seleccionar");
}

// Mostrar pantalla de ajuste de mm por paso
void displayAdjustMmPerStep() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(5, 5);
  display.println("AJUSTE MM POR PASO");
  display.drawLine(0, 15, SCREEN_WIDTH, 15, WHITE);
  
  display.setCursor(10, 25);
  display.print("Valor actual: ");
  
  display.setTextSize(2);
  display.setCursor(20, 40);
  display.print(mmPerStep, 3);
  display.print(" mm");
  
  display.display();
  
  // Mostrar la misma información en el monitor serial
  Serial.println("\n📐 AJUSTE DE MM POR PASO");
  Serial.println("----------------------------------------------");
  Serial.print("Valor actual: ");
  Serial.print(mmPerStep, 3);
  Serial.println(" mm por paso");
  Serial.println("----------------------------------------------");
  Serial.println("Girar encoder para ajustar, pulsar para guardar");
}

// Mostrar pantalla de ajuste de distancia objetivo
void displayAdjustTargetDistance() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(5, 5);
  display.println("AJUSTE DIST. OBJETIVO");
  display.drawLine(0, 15, SCREEN_WIDTH, 15, WHITE);
  
  display.setCursor(10, 25);
  display.print("Valor actual: ");
  
  display.setTextSize(2);
  display.setCursor(20, 40);
  display.print(TARGET_DISTANCE, 1);
  display.print(" mm");
  
  display.display();
  
  // Mostrar la misma información en el monitor serial
  Serial.println("\n🎯 AJUSTE DE DISTANCIA OBJETIVO");
  Serial.println("----------------------------------------------");
  Serial.print("Valor actual: ");
  Serial.print(TARGET_DISTANCE, 1);
  Serial.println(" mm");
  Serial.println("----------------------------------------------");
  Serial.println("Girar encoder para ajustar, pulsar para guardar");
}

// Función para guardar valores en Preferences
void savePreferences() {
  preferences.putLong("encoderCount", encoderCount);
  preferences.putFloat("targetDist", TARGET_DISTANCE);
  preferences.putFloat("mmPerStep", mmPerStep);
  
  Serial.println("\n💾 GUARDANDO EN MEMORIA");
  Serial.println("----------------------------------------------");
  Serial.println("✅ Valores guardados con éxito:");
  Serial.print("   - Posición actual (pasos): ");
  Serial.println(encoderCount);
  Serial.print("   - Distancia objetivo: ");
  Serial.print(TARGET_DISTANCE, 1);
  Serial.println(" mm");
  Serial.print("   - mm por paso: ");
  Serial.print(mmPerStep, 3);
  Serial.println(" mm");
  Serial.println("----------------------------------------------");
}

// Función para cargar valores desde Preferences
void loadPreferences() {
  encoderCount = preferences.getLong("encoderCount", 0);  // Valor predeterminado = 0
  TARGET_DISTANCE = preferences.getFloat("targetDist", 50.0);  // Valor por defecto = 50.0 mm
  mmPerStep = preferences.getFloat("mmPerStep", 0.5);  // Valor por defecto = 0.5 mm
  
  Serial.println("\n📂 CARGANDO DESDE MEMORIA");
  Serial.println("----------------------------------------------");
  Serial.print("📡 Posición actual (pasos): ");
  Serial.println(encoderCount);
  Serial.print("📏 Distancia actual calculada: ");
  Serial.print(encoderCount * mmPerStep, 2);
  Serial.println(" mm");
  Serial.print("🎯 Distancia objetivo: ");
  Serial.print(TARGET_DISTANCE, 1);
  Serial.println(" mm");
  Serial.print("📐 mm por paso: ");
  Serial.print(mmPerStep, 3);
  Serial.println(" mm");
  Serial.println("----------------------------------------------");
}

// Procesar input del encoder en el modo de configuración
void processEncoderConfigMode(int direction) {
  switch (currentState) {
    case CONFIG_MENU:
      // Navegar por el menú
      menuSelection += direction;
      // Mantener la selección dentro de los límites
      if (menuSelection < 0) menuSelection = MAX_MENU_ITEMS - 1;
      if (menuSelection >= MAX_MENU_ITEMS) menuSelection = 0;
      
      displayConfigMenu();
      break;
      
    case ADJUST_MM_PER_STEP:
      // Ajustar mmPerStep (incrementos más pequeños)
      mmPerStep += direction * 0.01;
      if (mmPerStep < 0.01) mmPerStep = 0.01; // Evitar valores negativos o cero
      
      displayAdjustMmPerStep();
      break;
      
    case ADJUST_TARGET_DISTANCE:
      // Ajustar TARGET_DISTANCE (incrementos de 1mm)
      TARGET_DISTANCE += direction;
      if (TARGET_DISTANCE < 1) TARGET_DISTANCE = 1; // Evitar valores negativos o cero
      
      displayAdjustTargetDistance();
      break;
      
    default:
      break;
  }
}

// Procesar pulsación del botón en el modo de configuración
void processButtonConfigMode() {
  switch (currentState) {
    case CONFIG_MENU:
      // Seleccionar opción del menú
      if (menuSelection == 0) {
        currentState = ADJUST_MM_PER_STEP;
        Serial.println("\n🔄 Seleccionado: Ajustar mm por paso");
        displayAdjustMmPerStep();
      } else if (menuSelection == 1) {
        currentState = ADJUST_TARGET_DISTANCE;
        Serial.println("\n🔄 Seleccionado: Ajustar distancia objetivo");
        displayAdjustTargetDistance();
      }
      break;
      
    case ADJUST_MM_PER_STEP:
    case ADJUST_TARGET_DISTANCE:
      // Guardar y volver al menú
      savePreferences();
      currentState = CONFIG_MENU;
      Serial.println("🔙 Volviendo al menú principal");
      displayConfigMenu();
      break;
      
    default:
      break;
  }
}

void loop() {
  // Leer estado actual del CLK
  int currentStateCLK = digitalRead(ENCODER_CLK);
  
  // Verificar botón de configuración
  if (digitalRead(CONFIG_BUTTON) == LOW) {
    if (millis() - lastButtonPress > debounceTime) {
      lastButtonPress = millis();
      
      if (currentState == NORMAL_MODE) {
        // Entrar al modo configuración
        currentState = CONFIG_MENU;
        menuSelection = 0;
        Serial.println("\n🔧 ENTRANDO EN MODO CONFIGURACIÓN");
        displayConfigMenu();
      } else {
        // Salir del modo configuración y volver al modo normal
        currentState = NORMAL_MODE;
        Serial.println("\n🔄 SALIENDO DE CONFIGURACIÓN - VOLVIENDO A MODO NORMAL");
        // Mostrar la distancia actual
        displayDistance(encoderCount * mmPerStep);
      }
    }
  }

  // Verificar si hubo un cambio en el estado del CLK (flanco descendente)
  if (currentStateCLK != lastStateCLK && currentStateCLK == LOW) {
    // Leer estado del pin DT para determinar dirección
    int stateDT = digitalRead(ENCODER_DT);
    int direction = (stateDT == HIGH) ? 1 : -1;
    
    if (currentState == NORMAL_MODE) {
      // Modo normal: contar distancia
      encoderCount += direction;
      
      // Calcular distancia en mm
      float distanceMM = encoderCount * mmPerStep;
      
      // Mostrar distancia actualizada en el OLED
      displayDistance(distanceMM);
      
      // Verificar si alcanzamos la distancia objetivo para encender/apagar el LED
      if (distanceMM >= TARGET_DISTANCE) {
        digitalWrite(LED_PIN, HIGH);  // Encender LED
      } else {
        digitalWrite(LED_PIN, LOW);   // Apagar LED
      }
      
      // Guardar valores actualizados en Preferences
      preferences.putLong("encoderCount", encoderCount); // Solo guardamos la posición para evitar spam
    } 
    else {
      // Modo configuración: ajustar parámetros
      processEncoderConfigMode(direction);
    }
  }

  // Actualizar estado del CLK para la siguiente lectura
  lastStateCLK = currentStateCLK;

  // Verificar si el botón del encoder fue presionado
  if (digitalRead(ENCODER_SW) == LOW) {
    // Verificar tiempo para evitar rebote
    if (millis() - lastButtonPress > debounceTime) {
      lastButtonPress = millis();
      
      if (currentState == NORMAL_MODE) {
        // Reiniciar contador en modo normal
        encoderCount = 0;
        Serial.println("\n🔄 REINICIANDO CONTADOR A CERO");
        displayDistance(0.0);
        digitalWrite(LED_PIN, LOW);
        savePreferences();
      } 
      else {
        // Procesar acción del botón en modo configuración
        processButtonConfigMode();
      }
    }
  }
}