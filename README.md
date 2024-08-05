# Deploy
1. Prender máquina y ver IP
2. Abrir Putty (user: utnso - password: utnso)
3. git clone https://github.com/sisoputnfrba/so-deploy.git
4. cd so-deploy
5. ./deploy.sh -r=release -p=utils -p=kernel -p=cpu -p=memoria -p=entradasalida "tp-2024-1c-La-Tercera"
6. user: elpanchi05 - token: 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
7. cd tp-2024-1c-La-Tercera
8. nano config/ip.config
9. config_mod.sh -prueba- -paso- (o a mano: nano config/modulo.config)
10. ./run.sh


# Deploy (backup)
0. user: elpanchi05 - token: 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
1. Averigusr las IPS de cada MV (con ifconfig)
2. Abrir Putty (user: utnso - password: utnso)
3. git clone https://github.com/sisoputnfrba/tp-2024-1c-La-Tercera.git
5. cd tp-2024-1c-La-Tercera
6. ./deploy.sh tp-2024-1c-La-Tercera
7. cd config
8. nano ip.config
9. config_mod.sh -nombre_de_prueba- -paso-
10. ./run.sh - Para IO: ./run.sh -prueba- -numero_de_io- (o a mano: ./bin/entradasalida ../config/interfaz.config NOMBRE_INTERFAZ)

# Comandos Kernel
1. EJECUTAR_SCRIPT ../tests/scripts/PRUEBA_PLANI
------
2. EJECUTAR_SCRIPT ../tests/scripts/PRUEBA_DEADLOCK
------
3. 1. INICIAR_PROCESO ../tests/instructions/MEMORIA_1
   2. INICIAR_PROCESO ../tests/instructions/MEMORIA_2
   3. INICIAR_PROCESO ../tests/instructions/MEMORIA_3
------
4. EJECUTAR_SCRIPT ../tests/scripts/PRUEBA_IO
------
5. 1. INICIAR_PROCESO ../tests/instructions/FS_1
   2. INICIAR_PROCESO ../tests/instructions/FS_2
   3. INICIAR_PROCESO ../tests/instructions/FS_3
   4. INICIAR_PROCESO ../tests/instructions/FS_4
------
6. EJECUTAR_SCRIPT ../tests/scripts/PRUEBA_SALVATIONS_EDGE

## Dependencias

Para poder compilar y ejecutar el proyecto, es necesario tener instalada la
biblioteca [so-commons-library] de la cátedra:

```bash
git clone https://github.com/sisoputnfrba/so-commons-library
cd so-commons-library
make debug
make install
```

## Compilación

Cada módulo del proyecto se compila de forma independiente a través de un
archivo `makefile`. Para compilar un módulo, es necesario ejecutar el comando
`make` desde la carpeta correspondiente.

El ejecutable resultante se guardará en la carpeta `bin` del módulo.

## Importar desde Visual Studio Code

Para importar el workspace, debemos abrir el archivo `tp.code-workspace` desde
la interfaz o ejecutando el siguiente comando desde la carpeta raíz del
repositorio:

```bash
code tp.code-workspace
```

## Checkpoint

Para cada checkpoint de control obligatorio, se debe crear un tag en el
repositorio con el siguiente formato:

```
checkpoint-{número}
```

Donde `{número}` es el número del checkpoint.

Para crear un tag y subirlo al repositorio, podemos utilizar los siguientes
comandos:

```bash
git tag -a checkpoint-{número} -m "Checkpoint {número}"
git push origin checkpoint-{número}
```

Asegúrense de que el código compila y cumple con los requisitos del checkpoint
antes de subir el tag.

## Entrega

Para desplegar el proyecto en una máquina Ubuntu Server, podemos utilizar el
script [so-deploy] de la cátedra:

```bash
git clone https://github.com/sisoputnfrba/so-deploy.git
cd so-deploy
./deploy.sh -r=release -p=utils -p=kernel -p=cpu -p=memoria -p=entradasalida "tp-{año}-{cuatri}-{grupo}"
```

El mismo se encargará de instalar las Commons, clonar el repositorio del grupo
y compilar el proyecto en la máquina remota.

Ante cualquier duda, podés consultar la documentación en el repositorio de
[so-deploy], o utilizar el comando `./deploy.sh -h`.

[so-commons-library]: https://github.com/sisoputnfrba/so-commons-library
[so-deploy]: https://github.com/sisoputnfrba/so-deploy
