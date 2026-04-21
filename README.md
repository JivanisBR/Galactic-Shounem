## Galactic-Shounem
BIG Game Project jjjj


## 🚀​ Guia de Desenvolvimento:

​ Este documento contém as informações essenciais para você se localizar no código, entender a arquitetura atual e começar a fazer alterações.


## ​📁 1. Arquitetura do Código:

​Nosso jogo foi refatorado recentemente para usar Orientação a Objetos (OO) nas partes mais complexas como o Boss e entidades futuras.
O código está dividido em:

​SpaceGameplayOO.cpp (O Motor do SpaceShooter): 
É o motor mais refinado e testado até agora, mas futuramente será apenas uma gameplay de transição entre fases. Aqui rola o Game Loop principal, controle da nave do Player, spawn de meteoros e inimigos normais, física dos drops de minério, sistema de colisões geral e o gerador procedural do fundo de estrelas (parallax e nebulosas).

​Boss.h e Boss.cpp (A Caixa Preta do Chefão): 
Tudo relacionado ao Boss Final (texturas, IA, padrões de tiro, física das peças voando na morte, estados de raiva) está isolado nestes arquivos. O arquivo principal do jogo não sabe como o boss funciona, ele apenas instancia a classe (Boss* chefeFinal = new Boss();) e chama suas funções (ComportamentoVivo e ComportamentoMorto).

Exploration (jogotop.cpp):
É o motor da exploração top-down no planeta, onde ficará a loja, nave, eventos aleatórios futuros, e a entrada para o corredor de salas.

EstelarMap (Mapa do jogo):
É o mapa interativo da galáxia, onde o jogador irá selecionar para onde irá a seguir, e ver a localização de guerreiros fortes.

UpgradeAstral (árvore de skills):
É o motor de upgrade de skills baseada em quantas kills o jogador fez no game, e será o que definirá a aura e PDL do jogador.

BeatEmUp:
Será o motor principal do jogo, toda a mecânica de luta e movimentação do player e inimigos estará aqui futuramente.


## ​⚙️ 2. Como Fazer Ajustes Finos ("Tweaks")
​Muitos valores no jogo estão abertos para balanceamento e polimento.

Dica de ouro: Dê Ctrl+F na palavra "ajuste" pelo código. Você encontrará comentários indicando onde alterar cadência de tiro, gravidade das partículas e velocidades.

​Velocidade e Tempo: A variável global mult (baseada no GetFrameTime()) é o que mantém o jogo fluído independente dos frames. Qualquer coisa que se mova precisa ser multiplicada por mult ou GetFrameTime().

​Player Hitbox: A hitbox da nave não é do tamanho da imagem inteira. As variáveis death_x e death_y definem o miolo da nave.

​Sistema de Loot: Está na função SpawnLoot() no SpaceGameplayOO.cpp. Ali dá para alterar as chances de vir Ferro, Prata ou Ouro e o valor que cada meteoro dropa.


## ​⚙️ 2.5 Como Fazer Ajustes Baseados em Seeds Específicas ("Data-Driven Tweaks")
Na pasta Seeds de Teste haverá arquivos .txt que armazenam dados de rotas de gameplay(cenários) únicos e editáveis, para repetir alguma rota que ocorreu algum bug, ou para testar o level design e gamefeel geral daquela rota específica. 

É importante usar esta pasta para salvar rotas onde bugs de level aconteceram, e/ou para salvar rotas boas para testes com usuários. 

Para salvar uma Seed, deve copiar o arquivo viagem_data.txt da pasta SpaceShooter, e colar dentro da pasta Seeds de Teste, renomeando com a descrição da Seed. (ATENÇÃO: O arquivo salva apenas a última viagem gerada pelo mapa estelar). 

Para testar uma Seed, basta copiar o conteúdo dentro do arquivo da Seed, e colar dentro do arquivo viagem_data.txt da pasta SpaceShooter, e abrir o executável do SpaceGameplay.


## ​🛠️ 3. Funções Essenciais da Raylib Usadas

​Nós usamos a engine Raylib (C++). Se precisar mexer na parte visual ou de controles, estas são as funções que mais utilizamos:

​DrawTexturePro(): Desenha e manipula recortes de uma imagem maior (SpriteSheet). Usamos muito para animar a nave do player e as articulações do boss. Aceita escala e rotação.

​BeginBlendMode(BLEND_ADDITIVE) e EndBlendMode(): O segredo dos nossos gráficos! Tudo que precisa brilhar como laser, luzes neon ou fogo dos propulsores fica entre essas duas funções.

​IsKeyDown(KEY_...): Checa se a tecla está sendo segurada (usado para movimentação e turbo).

​IsKeyPressed(KEY_...): Checa se a tecla foi pressionada uma vez (usado para menus, reset ou disparos específicos).

​GetRandomValue(min, max): Usado exaustivamente na IA e na física de explosão para criar caos e imprevisibilidade procedural.


## ​💻 4. Como Compilar

​Se for testar localmente via terminal, usamos o compilador G++ incluindo todos os arquivos .cpp de uma vez. O comando padrão, feito no terminal das pastas específicas dentro da pasta Galactic Shounen é:

Para compilar o Mapa:
g++ *.cpp ../Shared/Player.cpp ../Shared/Nave.cpp -o MapaEstelar.exe -I ../Shared -I C:\raylib\raylib-5.5_win64_mingw-w64\include -L C:\raylib\raylib-5.5_win64_mingw-w64\lib -lraylib -lopengl32 -lgdi32 -lwinmm

Para compilar o SpaceShooter:
g++ *.cpp ../Shared/Player.cpp ../Shared/Nave.cpp -o SpaceGameplayOO.exe -I ../Shared -I C:\raylib\raylib-5.5_win64_mingw-w64\include -L C:\raylib\raylib-5.5_win64_mingw-w64\lib -lraylib -lopengl32 -lgdi32 -lwinmm


## 📚 5. Guia Rápido de Funções Raylib (Cheat Sheet)

Exemplos práticos de como preencher as principais funções visuais que usamos no código:

### Formas Geométricas Básicas
```cpp
// Desenha um retângulo sólido, usado para visualizar hitboxes.
// Parâmetros: Pos X, Pos Y, Largura, Altura, Cor
DrawRectangle(100, 50, 200, 100, RED);

// Desenha uma elipse (oval). Muito usado nos tiros e lasers.
// Parâmetros: Centro X, Centro Y, Raio Horizontal, Raio Vertical, Cor
DrawEllipse(300, 200, 4.0f, 15.0f, ORANGE);

// Desenha um círculo perfeito baseado em um Vector2 (Usado nos drops e partículas).
// Parâmetros: Posição (Vector2), Raio, Cor
DrawCircleV((Vector2){ 400.0f, 300.0f }, 8.0f, GOLD);


// Escreve um texto fixo na tela.
// Parâmetros: Texto, Pos X, Pos Y, Tamanho da Fonte, Cor
DrawText("PRESS R TO RESPAWN", 450, 420, 20, RED);

// Para misturar texto com variáveis numéricas (como Vida ou Pontos), use TextFormat:
DrawText(TextFormat("Vida: %d", vidaBoss), 100, 50, 20, WHITE);


// Desenha uma imagem inteira na tela.
// Parâmetros: Textura, Pos X, Pos Y, Cor de Filtro (WHITE mantém as cores originais)
DrawTexture(meteoro_tex, meteoro_x, meteoro_y, WHITE);

// DrawTexturePro: A função mais poderosa! Usada para animar a nave e montar o Boss.
// Ela corta um pedaço da imagem original, muda o tamanho e gira na tela.
Rectangle recorteImg = { 0.0f, 0.0f, 100.0f, 100.0f }; // X, Y, Largura, Altura no arquivo da imagem
Rectangle destinoTela = { (float)nave_x, (float)nave_y, 200.0f, 200.0f }; // Posição e Tamanho no jogo
Vector2 eixoRotacao = { 100.0f, 100.0f }; // Ponto central para o giro (Pivot)
float rotacao = 0.0f; // Ângulo em graus

// Parâmetros: Textura, Recorte (Source), Destino (Dest), Eixo (Origin), Rotação, Cor
DrawTexturePro(nav, recorteImg, destinoTela, eixoRotacao, rotacao, WHITE);


// Criando uma cor personalizada (RGBA: Vermelho, Verde, Azul, Transparência). Valores de 0 a 255.
Color roxoNeon = { 128, 0, 128, 255 }; 

// ColorAlpha: Essencial nas explosões e animação de morte para fazer o objeto "sumir" aos poucos.
// Parâmetros: Cor base, Transparência em float (0.0f é 100% invisível, 1.0f é 100% sólido)
ColorAlpha(RED, 0.5f); // Retorna um vermelho com 50% de transparência

