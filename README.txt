======================================================
  QUORIDOR PAC-MAN — LP1 TP Final
  Universidad Católica — Lenguajes de Programación 1
======================================================

VERSIÓN DE UN SOLO ARCHIVO: todo el código está en
quoridor_pacman.c, organizado en secciones numeradas
(tipos, tablero, mapas, lógica, IA, dibujado, input, main).

CÓMO COMPILAR
-------------
Necesitás tener raylib instalado. Luego ejecutar:

  make

O directamente con gcc (Linux):
  gcc quoridor_pacman.c -lraylib -lGL -lm -lpthread -ldl -lrt -lX11 -o quoridor_pacman

En Windows con la versión de raylib para MinGW:
  gcc quoridor_pacman.c -I. -Lraylib/lib -lraylib -lopengl32 -lgdi32 -lwinmm -o quoridor_pacman.exe

IMPORTANTE: ejecutar el programa desde la carpeta del proyecto,
porque busca los mapas en la subcarpeta maps/.

CÓMO JUGAR
----------
1. En el menú principal presioná [1] para jugar.
2. En la pantalla de configuración ajustá los parámetros
   y presioná [Enter] para empezar.

CONTROLES - PAC-MAN (jugador 1):
  Flechas           : mover Pac-Man (1 casilla = 1 acción)
  M                 : entrar modo colocación de muro
  En modo muro: Flechas para elegir dirección, Enter para confirmar
  Enter             : terminar turno antes de agotar las acciones
  R                 : reiniciar partida
  ESC               : volver al menú

CONTROLES - FANTASMAS en modo Player vs Player (jugador 2):
  W/A/S/D           : mover el fantasma activo (la movida termina
                      el turno automaticamente, porque un fantasma
                      tiene una sola accion por turno)
  M                 : entrar en modo muro; despues W/A/S/D elige de
                      que lado del fantasma se coloca (usa el turno)
  Tab               : pasar el turno sin hacer nada

EN LA PANTALLA DE CONFIGURACION:
  G                 : alternar IA vs Jugador / Jugador vs Jugador
  1-4               : habilitar/deshabilitar Blinky/Inky/Pinky/Clyde
  5-8               : ciclar la dificultad de cada fantasma
                      (independiente para cada uno)
  +/-               : muros en la mano de Pac-Man
  W/S               : muros en la mano de los fantasmas
  A/D               : tiempo de vida de los muros (en turnos)
  M                 : elegir mapa
  Enter             : empezar la partida

CÓMO USAR EL EDITOR
--------------------
1. En el menú presioná [2] para abrir el editor.
2. Elegí el tamaño del tablero:
     T / G = agregar / quitar filas
     Y / H = agregar / quitar columnas
   (mínimo 5×5, máximo 20×20)
3. Usá las flechas para mover el cursor por la grilla.
4. Teclas para colocar fichas:
     P = Pac-Man
     B = Blinky (rojo)
     I = Inky (cian)
     N = Pinky (rosa)
     C = Clyde (naranja)
     1-4 = pac-bola 1, 2, 3 ó 4
5. Para muros permanentes:
     W = muro horizontal hacia abajo desde el cursor
     V = muro vertical hacia la derecha desde el cursor
     X = deshacer último muro
6. Presioná S para guardar, escribí el nombre y Enter.
   El mapa se guarda en maps/<nombre>.map y aparece
   automáticamente en la pantalla de selección de mapas.

DIFICULTAD DE CADA FANTASMA
----------------------------
La dificultad determina cómo se comporta la IA del fantasma:

  FÁCIL (1):
    El fantasma solo sigue el camino óptimo hacia Pac-Man
    el 30% de las veces. El 70% restante se mueve en una
    dirección aleatoria. Es fácil de evadir.

  MEDIO (2):
    El 60% de las veces sigue el camino óptimo (BFS).
    El 40% restante es aleatorio. Balance razonable.

  DIFÍCIL (3):
    Siempre sigue el camino más corto hacia Pac-Man (BFS
    100% del tiempo). Además, cuando tiene muros disponibles
    y ve a Pac-Man en línea recta, coloca un muro para
    bloquearlo. Es muy difícil de evadir.

La dificultad solo aplica en modo IA vs Jugador. En modo
Player vs Player, los fantasmas los maneja el segundo jugador
y la dificultad no tiene efecto.

DECISIONES DE DISEÑO
--------------------
- Los muros temporales se colocan pegados a la ficha propia
  (Pac-Man o el fantasma activo eligen de qué lado de su casilla).
  El enunciado no fija dónde se pueden colocar; elegimos esta regla
  porque hace la colocación rápida con teclado.
- Un "turno global" es una ronda completa (Pac-Man + todos los
  fantasmas). Los contadores de los muros temporales bajan una vez
  por ronda.

MAPAS PREDEFINIDOS
------------------
  map1.map — 9×9 clásico. Tablero estándar con muros en el centro.
  map2.map — 7×7 compacto. Partidas más cortas e intensas.
  map3.map — 11×11 abierto. Más espacio, modo frenético más raro.

NIVELES
-------
Al terminar la partida se muestra el nivel alcanzado:
  1 pac-bola = Pac-Man novato
  2 pac-bolas = Pac-Man prometedor
  3 pac-bolas = Pac-Man de categoría
  4 pac-bolas = Pac-Man de élite

======================================================
