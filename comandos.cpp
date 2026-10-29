#include "comandos.h"

#include <ctype.h>

#include "config.h"
#include "motores.h"

namespace {

struct ParserEstado {
  Stream* stream = nullptr;
  char buffer[32] = {0};
  uint8_t longitud = 0;
  unsigned long ultimoCharMs = 0;
  bool descartar = false;
};

// Un parser independiente para USB y otro para Bluetooth. Así los mensajes
// de estado del HC-05 (+READY, +CONNECTED, etc.) no se mezclan con comandos.
ParserEstado parsers[2];

ParserEstado& obtenerParser(Stream& entrada) {
  for (uint8_t i = 0; i < 2; ++i) {
    if (parsers[i].stream == &entrada) {
      return parsers[i];
    }
  }
  for (uint8_t i = 0; i < 2; ++i) {
    if (parsers[i].stream == nullptr) {
      parsers[i].stream = &entrada;
      return parsers[i];
    }
  }
  return parsers[0];
}

void imprimirLinea(Stream& salida, bool eco,
                   const __FlashStringHelper* texto) {
  if (eco) {
    salida.println(texto);
  } else {
    Serial.println(texto);
  }
}

void imprimirEstado(Stream& salida, bool eco) {
  String estado = F("Estado motores: ");
  estado += motoresEstadoTexto();
  Serial.println(estado);
  if (eco) {
    salida.println(estado);
  }
}

void imprimirAyuda(Stream& salida, bool eco) {
  imprimirLinea(salida, eco,
                F("Comandos: a=adelante r=atras i=izquierda d=derecha"));
  imprimirLinea(salida, eco, F("s=detener p=demo m=estado h=ayuda"));
  imprimirLinea(salida, eco,
                F("a/r/i/d siguen hasta s o hasta 5 s sin una nueva orden."));
}

void ejecutarComando(const String& token, Stream& entrada, bool eco) {
  if (token.length() == 0) {
    return;
  }

  // Comandos de una letra.
  if (token.length() == 1) {
    const char comando = token[0];

    switch (comando) {
      case 'a':
        motoresControlContinuo(DireccionMovimiento::ADELANTE);
        if (eco) {
          entrada.println(F("OK: adelante (s=detener)"));
        }
        return;
      case 'r':
        motoresControlContinuo(DireccionMovimiento::ATRAS);
        if (eco) {
          entrada.println(F("OK: atras (s=detener)"));
        }
        return;
      case 'i':
        motoresControlContinuo(DireccionMovimiento::IZQUIERDA);
        if (eco) {
          entrada.println(F("OK: izquierda (s=detener)"));
        }
        return;
      case 'd':
        motoresControlContinuo(DireccionMovimiento::DERECHA);
        if (eco) {
          entrada.println(F("OK: derecha (s=detener)"));
        }
        return;
      case 's':
        motoresDetener();
        if (eco) {
          entrada.println(F("OK: detenido"));
        }
        return;
      case 'p':
        motoresDemo();
        if (eco) {
          entrada.println(F("OK: demo iniciada"));
        }
        return;
      case 'm':
        imprimirEstado(entrada, eco);
        return;
      case 'h':
      case '?':
        imprimirAyuda(entrada, eco);
        return;
      default:
        // Se ignora en silencio: puede ser parte de un mensaje del módulo.
        return;
    }
  }

  // También se aceptan palabras completas, útiles en algunas apps.
  if (token == "adelante") {
    motoresControlContinuo(DireccionMovimiento::ADELANTE);
    if (eco) {
      entrada.println(F("OK: adelante (s=detener)"));
    }
  } else if (token == "atras") {
    motoresControlContinuo(DireccionMovimiento::ATRAS);
    if (eco) {
      entrada.println(F("OK: atras (s=detener)"));
    }
  } else if (token == "izquierda") {
    motoresControlContinuo(DireccionMovimiento::IZQUIERDA);
    if (eco) {
      entrada.println(F("OK: izquierda (s=detener)"));
    }
  } else if (token == "derecha") {
    motoresControlContinuo(DireccionMovimiento::DERECHA);
    if (eco) {
      entrada.println(F("OK: derecha (s=detener)"));
    }
  } else if (token == "detener" || token == "stop") {
    motoresDetener();
    if (eco) {
      entrada.println(F("OK: detenido"));
    }
  } else if (token == "demo") {
    motoresDemo();
    if (eco) {
      entrada.println(F("OK: demo iniciada"));
    }
  } else if (token == "estado") {
    imprimirEstado(entrada, eco);
  } else if (token == "ayuda") {
    imprimirAyuda(entrada, eco);
  }
  // Cualquier otra cadena (por ejemplo +READY) se ignora.
}

void procesarBuffer(ParserEstado& parser, Stream& entrada, bool eco,
                    const char* origen) {
  parser.buffer[parser.longitud] = '\0';

  String token = String(parser.buffer);
  token.trim();
  token.toLowerCase();

  parser.longitud = 0;
  parser.buffer[0] = '\0';

  if (token.length() == 0) {
    return;
  }

  Serial.printf("[%s] Comando: %s\n", origen, token.c_str());
  ejecutarComando(token, entrada, eco);
}

}  // namespace

void comandosProcesar(Stream& entrada, const char* origen, bool eco) {
  ParserEstado& parser = obtenerParser(entrada);

  while (entrada.available() > 0) {
    char crudo = static_cast<char>(entrada.read());

    if (crudo == '\r' || crudo == '\n') {
      if (!parser.descartar) {
        procesarBuffer(parser, entrada, eco, origen);
      }
      parser.descartar = false;
      parser.longitud = 0;
      parser.buffer[0] = '\0';
      parser.ultimoCharMs = millis();
      continue;
    }

    if (parser.descartar) {
      parser.ultimoCharMs = millis();
      continue;
    }

    if (crudo == '\t') {
      crudo = ' ';
    }

    if (parser.longitud < sizeof(parser.buffer) - 1) {
      parser.buffer[parser.longitud++] = crudo;
    } else {
      // Cadena demasiado larga: se descarta hasta el próximo salto de línea.
      parser.descartar = true;
      parser.longitud = 0;
      parser.buffer[0] = '\0';
    }
    parser.ultimoCharMs = millis();
  }

  // Permite enviar una sola letra sin Enter. Se espera un breve silencio para
  // no confundirla con los mensajes de estado del módulo (+READY, etc.).
  if (!parser.descartar && parser.longitud > 0 &&
      (millis() - parser.ultimoCharMs >= 60UL)) {
    procesarBuffer(parser, entrada, eco, origen);
  }
}
