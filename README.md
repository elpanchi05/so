# Sistema operativo
Desarrollado en C.
Cuatro módulos que trabajan en conjunto para ejecutar pseudoinstrucciones. Los módulos son: kernel, cpu, memoria y entrada/salida (filesystem incluido).

# Deploy
1. ./deploy.sh -r=release -p=utils -p=kernel -p=cpu -p=memoria -p=entradasalida "tp-2024-1c-La-Tercera"
2. config_mod.sh -prueba- -paso- (o a mano: nano config/modulo.config)
3. modulo/run.sh

[so-commons-library]: https://github.com/sisoputnfrba/so-commons-library
[so-deploy]: https://github.com/sisoputnfrba/so-deploy
