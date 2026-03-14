# Galactic-Shounem
BIG Game Project jjjj


🚀​ Guia de Desenvolvimento:

​ Este documento contém as informações essenciais para você se localizar no código, entender a arquitetura atual e começar a fazer alterações.


​📁 1. Arquitetura do Código:

​Nosso jogo foi refatorado recentemente para usar Orientação a Objetos (OO) nas partes mais complexas como o Boss e entidades futuras.
O código está dividido em:
​SpaceGameplayOO.cpp (O Motor Principal): 
É o coração do jogo. Aqui rola o Game Loop principal, controle da nave do Player, spawn de meteoros e inimigos normais, física dos drops de minério, sistema de colisões geral e o gerador procedural do fundo de estrelas (parallax e nebulosas).
​Boss.h e Boss.cpp (A Caixa Preta do Chefão): Tudo relacionado ao Boss Final (texturas, IA, padrões de tiro, física das peças voando na morte, estados de raiva) está isolado nestes arquivos. O arquivo principal do jogo não sabe como o boss funciona, ele apenas instancia a classe (Boss* chefeFinal = new Boss();) e chama suas funções (ComportamentoVivo e ComportamentoMorto).

​⚙️ 2. Como Fazer Ajustes Finos ("Tweaks")
​Muitos valores no jogo estão abertos para balanceamento e polimento.
Dica de ouro: Dê Ctrl+F na palavra "ajuste" pelo código. Você encontrará comentários indicando onde alterar cadência de tiro, gravidade das partículas e velocidades.
​Velocidade e Tempo: A variável global mult (baseada no GetFrameTime()) é o que mantém o jogo fluído independente dos frames. Qualquer coisa que se mova precisa ser multiplicada por mult ou GetFrameTime().
​Player Hitbox: A hitbox da nave não é do tamanho da imagem inteira. As variáveis death_x e death_y definem o miolo da nave.
​Sistema de Loot: Está na função SpawnLoot() no SpaceGameplayOO.cpp. Ali dá para alterar as chances de vir Ferro, Prata ou Ouro e o valor que cada meteoro dropa.


​🛠️ 3. Funções Essenciais da Raylib Usadas
​Nós usamos a engine Raylib (C++). Se precisar mexer na parte visual ou de controles, estas são as funções que mais utilizamos:

​DrawTexturePro(): Desenha e manipula recortes de uma imagem maior (SpriteSheet). Usamos muito para animar a nave do player e as articulações do boss. Aceita escala e rotação.
​BeginBlendMode(BLEND_ADDITIVE) e EndBlendMode(): O segredo dos nossos gráficos! Tudo que precisa brilhar como laser, luzes neon ou fogo dos propulsores fica entre essas duas funções.
​IsKeyDown(KEY_...): Checa se a tecla está sendo segurada (usado para movimentação e turbo).
​IsKeyPressed(KEY_...): Checa se a tecla foi pressionada uma vez (usado para menus, reset ou disparos específicos).
​GetRandomValue(min, max): Usado exaustivamente na IA e na física de explosão para criar caos e imprevisibilidade.


​💻 4. Como Compilar

​Se for testar localmente via terminal, usamos o compilador G++ incluindo todos os arquivos .cpp de uma vez. O comando padrão (ajuste os caminhos da Raylib conforme o seu PC) é:
g++ *.cpp -o SpaceGameplayOO.exe -I C:\raylib\raylib-5.5_win64_mingw-w64\include -L C:\raylib\raylib-5.5_win64_mingw-w64\lib -lraylib -lopengl32 -lgdi32 -lwinmm

