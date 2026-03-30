#include "raylib.h"
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include "Player.h"
#include "Nave.h"

//----------------------------------------------------------------------------------
// ESTRUTURAS
//----------------------------------------------------------------------------------

struct Raio {
    std::vector<Vector2> pontos;
    int timer_troca;
    float espessura;
};

struct ParticulaAura {
    Vector2 offset; 
    float vida;
    Vector2 velocidade;
    float tam;
    Color cor;
    bool negativa;
};

// Estados possíveis do Ki
enum EstadoKi {
    ESCONDIDO,
    ELEVANDO,   // Crescendo (Spawning)
    MAXIMO,     // Totalmente visível
    SUPRIMINDO  // Diminuindo (Despawning)
};

struct Planeta {
    std::string nome;
    std::string desc_vida;
    int tipo_vida;
};

struct Estrela {
    // --- DADOS FÍSICOS DA ESTRELA ---
    Vector2 pos;
    int tam_nucleo;     
    bool crescendo;
    int timer_anim;
    int limite_anim;
    int tam_base;
    int alcance_cresc;

    // --- Dados do Sistema Estelar (Procedural) ---
    bool sistema_gerado = false;
    std::string classificacao_cientifica;
    int idade_milhoes_anos;
    std::vector<Planeta> planetas;

    // --- DADOS DO BOSS/INIMIGO (Mudam de estrela pra estrela) ---
    bool tem_chefe;
    std::string nome_chefe; 
    
    float diametro_maximo;
    int nivel_maximo;      
    int nivel_base;        
    int nivel_atual;       
    Color cor_aura;         
    bool eh_lendario;       

    EstadoKi estado_ki;
    float escala_atual;  
    float escala_minima; 
    int timer_estado;    
    
    std::vector<Raio> raios;
    std::vector<ParticulaAura> particulas_ki;

    // --- NOVO: SISTEMA DE VIAGEM ESTELAR ---
    int timer_viagem;        // Contagem regressiva para decidir pular
    int id_destino;          // Índice da estrela no vetor (-1 se parado)
    float progresso_viagem;  // 0.0f a 1.0f (Interpolação)
    Vector2 pos_visual_ki;   // A aura pode estar "no meio do caminho"
};

// Área de Risco
struct ZonaMeteoro {
    Vector2 pos;
    float raio;
};

struct Nebulosa {
    Vector2 pos;
    Color cor_base;
    std::vector<Vector2> offsets_particulas; // Offsets fixos
    std::vector<float> tamanhos;
    std::vector<int> alphas;
};

// Estrutura para galáxias distantes de fundo
struct BackgroundGalaxy {
    Vector2 pos;           // Posição no mundo (longe do centro)
    float rotation;        // Ângulo atual
    float rotSpeed;        // Velocidade de rotação (muito lenta)
    int textureIndex;      // Qual imagem usar (0, 1, 2, 3...)
    float scale;           // Variação de tamanho
    float pulseOffset;     // Para elas não pulsarem sincronizadas
    float pulseSpeed;      // Velocidade do brilho
};

// Estrelas decorativas super brilhantes
struct EstrelaBrilhante {
    Vector2 pos;
    float tamanho;       // Tamanho base
    float alphaBase;     // Transparência base
    float velocidadePisca; 
    float offsetPisca;   // Para não piscarem todas iguais
    Color cor;           // Variação sutil (azulada, amarelada)
};


//----------------------------------------------------------------------------------
// BANCO DE DADOS
//----------------------------------------------------------------------------------
const std::vector<std::string> NOMES_BASE = {
    "Draggster", "Monzer", "Zarbonn", "Numblerr", "Mysterr", 
    "Klebber", "Dravkor", "Elevver", "Vorzar", "Fuskker", 
    "Taytan", "Chromx", "Lumminurr", "Umngger", "Corn", "Jacxson", 
    "Phantor", "Xeltrax", "Quorax", "Zenthur", "Vexilon", "Elbber",
    "Dyggor", "Leozzer", "Jaivvar", "Annar", "Dahmmyr", "Gordelhe",
    "Nicolas", "Kérg", "Kargor", "Vitty", "Mackonhorn", "Lepr", "Jivanners", "Ygorr", "Enders"
};

//----------------------------------------------------------------------------------
// FUNÇÕES AUXILIARES
//----------------------------------------------------------------------------------

std::string GerarNomeProcedural() {
    std::string nome = NOMES_BASE[GetRandomValue(0, NOMES_BASE.size() - 1)];
    if (GetRandomValue(0, 100) < 40) {
        nome += " " + NOMES_BASE[GetRandomValue(0, NOMES_BASE.size() - 1)];
    }
    return nome;
}

// Função de Raios Unificada (A escala define agressividade e espessura)
void RegenerarRaio(Raio& r, float raio_base, int min_seg, int max_seg, float escala_forca) {
    r.pontos.clear();
    
    int segmentos = GetRandomValue(min_seg, max_seg);
    
    float angulo_base = (float)GetRandomValue(0, 360) * DEG2RAD;
    float dist = raio_base * 0.4f;
    Vector2 atual = { cosf(angulo_base) * dist, sinf(angulo_base) * dist };
    r.pontos.push_back(atual);

    // O offset base (o quão "tremido" é o raio) escala com a força
    int limite_offset = (int)(15.0f * escala_forca); 

    for (int i = 0; i < segmentos; ++i) {
        atual.x += GetRandomValue(-limite_offset, limite_offset);
        atual.y += GetRandomValue(-limite_offset, limite_offset);
        r.pontos.push_back(atual);
    }
    
    r.timer_troca = GetRandomValue(5, 12); 
    r.espessura = escala_forca; // Salva a espessura para usar no DrawLineEx!
}

// Função que gera o sistema apenas sob demanda
void GerarSistemaEstelar(Estrela& e) {
    if (e.sistema_gerado) return; // Se já gerou, não faz de novo para não pesar

    // 1. A Ciência das Estrelas (Temperatura, Cor e Idade)
    int tipo = GetRandomValue(1, 100);
    
    if (tipo <= 5) { // 5% O/B - Azuis, Super Quentes, Jovens
        e.classificacao_cientifica = "Estrela Azul (Classe O)";
        e.idade_milhoes_anos = GetRandomValue(1, 50); 
    } else if (tipo <= 20) { // 15% A/F - Brancas
        e.classificacao_cientifica = "Estrela Branca (Classe A)";
        e.idade_milhoes_anos = GetRandomValue(100, 2000);
    } else if (tipo <= 40) { // 20% G - Amarelas (Como o Sol)
        e.classificacao_cientifica = "Ana Amarela (Classe G)";
        e.idade_milhoes_anos = GetRandomValue(2000, 8000);
    } else if (tipo <= 70) { // 30% K - Laranjas
        e.classificacao_cientifica = "Ana Laranja (Classe K)";
        e.idade_milhoes_anos = GetRandomValue(5000, 15000);
    } else { // 30% M - Vermelhas (Anãs antigas ou Gigantes Moribundas)
        if (GetRandomValue(0, 10) > 8) {
            e.classificacao_cientifica = "Gigante Vermelha";
            e.idade_milhoes_anos = GetRandomValue(8000, 12000); // Morrendo
        } else {
            e.classificacao_cientifica = "Ana Vermelha (Classe M)";
            e.idade_milhoes_anos = GetRandomValue(10000, 50000); // Vivem muito
        }
    }

    // 2. Geração Procedural dos Planetas
    int numPlanetas = GetRandomValue(1, 5);
    int planetaHabitavelIndex = -1;

    // Se a estrela tem um chefe, o boss PRECISA estar em um planeta.
    // Se não tem, damos apenas 10% de chance de ter um planeta vivo pacífico.
    if (e.tem_chefe || GetRandomValue(1, 100) <= 10) {
        planetaHabitavelIndex = GetRandomValue(0, numPlanetas - 1);
    }

    for (int i = 0; i < numPlanetas; i++) {
        Planeta p;
        int letras = GetRandomValue(4, 6);
        p.nome = "";
        for (int j = 0; j < letras; j++) {
            p.nome += (char)GetRandomValue(65, 90); 
        }
        p.nome += "-" + std::to_string(GetRandomValue(0, 100));

        // Define a Vida e o Tipo
        if (i == planetaHabitavelIndex) {
            p.tipo_vida = GetRandomValue(2, 4);
            if (p.tipo_vida == 2) p.desc_vida = "Civilizacao Primitiva";
            else if (p.tipo_vida == 3) p.desc_vida = "Civilizacao Moderna";
            else p.desc_vida = "Civilizacao Avancada";
        } else {
            p.tipo_vida = 1;
            p.desc_vida = "Incompativel";
        }

        e.planetas.push_back(p);
    }

    e.sistema_gerado = true;
}

// Configura uma estrela já criada com dados de RPG
void ConfigurarBoss(Estrela& e) {
    int chance = GetRandomValue(0, 100);
    e.tem_chefe = false;
    e.eh_lendario = false; // Será redefinido abaixo
    e.diametro_maximo = 0;
    e.raios.clear();
    e.particulas_ki.clear();
    e.timer_estado = GetRandomValue(100, 1000); 

    // Chance global de ter chefe (mantive 70% de chance de NÃO ter, ou seja, rola dado > 30)
    // Se quiser que SPAWNE chefe com as chances que você disse, assumo que chance = 100% de ter algo?
    // Vou assumir a sua regra de spawn para QUALIFICAR o chefe caso ele exista.
    
    if (GetRandomValue(0, 100) < 30) { // 30% de chance de ter um inimigo na estrela
        e.tem_chefe = true;
        e.nome_chefe = GerarNomeProcedural();
        e.cor_aura = ColorFromHSV((float)GetRandomValue(0, 360), 0.8f, 0.9f); 

        // --- HIERARQUIA DE PODER (Sua nova regra) ---
        int roll = GetRandomValue(0, 100);

        if (roll <= 70) {
            // Tier 1: Max 500k
            e.nivel_maximo = GetRandomValue(10000, 500000);
            e.diametro_maximo = (float)GetRandomValue(80, 150);
        }
        else if (roll <= 85) {
            // Tier 2: Max 1M
            e.nivel_maximo = GetRandomValue(500001, 1000000);
            e.diametro_maximo = (float)GetRandomValue(150, 250);
        }
        else if (roll <= 90) {
            // Tier 3: Max 1.5M
            e.nivel_maximo = GetRandomValue(1000001, 1500000);
            e.diametro_maximo = (float)GetRandomValue(250, 350);
        }
        else if (roll <= 94) {
            // Tier 4: Max 2M
            e.nivel_maximo = GetRandomValue(1500001, 2000000);
            e.diametro_maximo = (float)GetRandomValue(350, 500);
            e.eh_lendario = true;
            e.nome_chefe = "GENERAL " + e.nome_chefe;
        }
        else if (roll <= 98) {
            // Tier 5: Max 2.5M (Divino)
            e.nivel_maximo = GetRandomValue(2000001, 2500000);
            e.diametro_maximo = (float)GetRandomValue(500, 700);
            e.eh_lendario = true;
            e.nome_chefe = "DEUS " + e.nome_chefe;
        }
        else {
            // TIER 6: Max 3M (ENTIDADE CÓSMICA) - NOVO!
            e.nivel_maximo = GetRandomValue(2500001, 3000000);
            e.diametro_maximo = (float)GetRandomValue(700, 1000); // Aura colossal
            e.eh_lendario = true;
            e.nome_chefe = "ENTIDADE " + e.nome_chefe;
        }

        // Configura Níveis Base
        float fator_supressao = (float)GetRandomValue(5, 20) / 100.0f; 
        e.nivel_base = (int)(e.nivel_maximo * fator_supressao);
        e.escala_minima = fator_supressao; if (e.escala_minima < 0.15f) e.escala_minima = 0.15f; 
        
        e.estado_ki = ESCONDIDO;
        e.escala_atual = e.escala_minima;
        e.nivel_atual = e.nivel_base;

        // --- QUANTIDADE DE EFEITOS (Proporcional ao Poder Máximo) ---
        // Inicialização dos Raios (Precisa passar o 5º argumento agora: 1.0f)
        int qtd_raios = 0;
        if (e.nivel_maximo > 1000000) qtd_raios = 4;
        if (e.nivel_maximo > 1500000) qtd_raios = 6;
        if (e.nivel_maximo > 2000000) qtd_raios = 8;
        if (e.nivel_maximo > 2500000) qtd_raios = 12; // Mais raios para a Entidade

        for(int i=0; i<qtd_raios; i++) { 
            Raio r; 
            // Inicializa com escala 1.0f por segurança
            RegenerarRaio(r, e.diametro_maximo/2, 3, 5, 1.0f); 
            e.raios.push_back(r); 
        }

        // Partículas (Proporcional: 1 partícula a cada 10.000 de poder maximo)
        // Ex: 500k = 50 particulas. 2.5M = 250 particulas.
        int qtd_part = e.nivel_maximo / 10000;
        if (qtd_part < 20) qtd_part = 20; // Mínimo
        if (qtd_part > 300) qtd_part = 300; // Teto de performance

        for(int i=0; i<qtd_part; i++) {
            ParticulaAura p;
            p.vida = 0; p.offset = {0, 0}; p.velocidade = {0, 0}; p.cor = WHITE; p.negativa = false;
            e.particulas_ki.push_back(p);
        }

    } else {
        e.nome_chefe = "Sistema Estelar";
        e.nivel_maximo = 0; e.nivel_atual = 0;
    }
}

void DesenharAuraComplexa(Vector2 centro, float raio, Color corBase) {
    float tempo = GetTime();
    
    // Cores pré-calculadas para otimização
    Color corCentro = ColorAlpha(WHITE, 0.4f);        // Núcleo quente
    Color corMedia = ColorAlpha(corBase, 0.3f);       // Corpo da energia
    Color corBorda = ColorAlpha(corBase, 0.1f);       // Fumaça/Brilho externo

    // 1. O GLOW BASE (O que você já tinha, mas menor, para ser o "volume")
    DrawCircleGradient(centro.x, centro.y, raio * 0.8f, corMedia, BLANK);

    // 2. CAMADA DE TURBULÊNCIA (O Segredo)
    // Desenhamos polígonos (triângulos/quadrados) girando para criar pontas irregulares
    
    // Camada A: Triângulo grande girando devagar (dá a forma geral de chama)
    DrawPoly(centro, 3, raio, tempo * 50.0f, corBorda);
    
    // Camada B: Quadrado médio girando rápido ao contrário (quebra a simetria)
    DrawPoly(centro, 4, raio * 0.85f, -tempo * 120.0f, corMedia);
    
    // Camada C: Outro triângulo defasado para preencher buracos
    DrawPoly(centro, 3, raio * 0.9f, tempo * 90.0f + 45.0f, corBorda);

    // 3. O NÚCLEO BRANCO PULSANTE
    // Um círculo menor no meio que "estoura" a cor para branco (blend add)
    float pulsoNucleo = 1.0f + sin(tempo * 10.0f) * 0.1f;
    DrawCircleGradient(centro.x, centro.y, (raio * 0.4f) * pulsoNucleo, corCentro, BLANK);
}

void DrawStarShape(Vector2 centro, float tamanho, Color cor)
{
    if (tamanho < 0.5f) return;
    // Cruz Principal
    DrawTriangle({centro.x, centro.y - tamanho}, {centro.x - tamanho * 0.2f, centro.y}, {centro.x + tamanho * 0.2f, centro.y}, cor);
    DrawTriangle({centro.x, centro.y + tamanho}, {centro.x + tamanho * 0.2f, centro.y}, {centro.x - tamanho * 0.2f, centro.y}, cor);
    DrawTriangle({centro.x - tamanho, centro.y}, {centro.x, centro.y + tamanho * 0.2f}, {centro.x, centro.y - tamanho * 0.2f}, cor);
    DrawTriangle({centro.x + tamanho, centro.y}, {centro.x, centro.y - tamanho * 0.2f}, {centro.x, centro.y + tamanho * 0.2f}, cor);
    // Centro
    DrawCircleV(centro, tamanho * 0.2f, cor);
}

// Função auxiliar para travar valores (Clamp)
float Clamp(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

// Função para girar um ponto ao redor do centro (0,0)
void RotacionarPonto(Vector2& p, float anguloGraus) {
    float rad = anguloGraus * (PI / 180.0f);
    float s = sinf(rad);
    float c = cosf(rad);
    
    // Armazena valores originais para não corromper o cálculo
    float x_antigo = p.x;
    float y_antigo = p.y;
    
    // Calcula novos valores baseados nos originais
    p.x = x_antigo * c - y_antigo * s;
    p.y = x_antigo * s + y_antigo * c;
}

// Variável de controle do visual
    bool mostrarVisual = true; // Começa ligado

// Controle da Rotação Galáctica
    float anguloGalaxia = 0.0f;
    const float VELOCIDADE_ROTACAO = -0.01f; // Muito lento e sutil (aumente para testar)

//----------------------------------------------------------------------------------
// MAIN
//----------------------------------------------------------------------------------
int main(void)
{
    // Tamanho do Mapa (Mundo)
    // 4000x3000 é um bom tamanho: cabe bastante coisa mas tem fim.
    const float MAP_WIDTH = 6000.0f;
    const float MAP_HEIGHT = 3500.0f;
    const int screenWidth = 1200;
    const int screenHeight = 700;
    
    InitWindow(screenWidth, screenHeight, "Mapa Galactico - Navegacao");
    SetTargetFPS(60);

    // --- CONFIGURAÇÃO DA CÂMERA ---
    Camera2D camera = { 0 };
    camera.target = { 0.0f, 0.0f }; // A câmera olha para o centro do universo (0,0)
    camera.offset = { screenWidth/2.0f, screenHeight/2.0f }; // O centro do universo é desenhado no meio da tela
    camera.rotation = 0.0f;
    camera.zoom = 0.5f; // Começa um pouco afastado
    bool cameraTravada = true;
    int focoCamera = -1; // -1 = Segue a Nave, >= 0 = Segue uma Estrela específica

    // --- GERAÇÃO DA GALÁXIA ---
    const int NUM_ESTRELAS = 450; // Quantidade fixa
    std::vector<Estrela> galaxia(NUM_ESTRELAS);
    std::vector<Nebulosa> nebulosas(20);
    std::vector<Vector2> poeira(1000); // Poeira de fundo fixa

    // 1. Gerar Poeira de Fundo (Espalhada numa área grande)
    for(auto& p : poeira) {
        p = { (float)GetRandomValue(-3000, 3000), (float)GetRandomValue(-3000, 3000) };
    }

    /*// 2. Gerar Nebulosas (Seguindo os Braços Espirais)
    // Usamos o vetor 'nebulosas' que já foi declarado lá em cima
    nebulosas.clear();
    // Recomendo aumentar um pouco a quantidade para cobrir os 5 braços (ex: 40 ou 50)
    int total_nebulosas = 20; 
    
    // PRECISAMOS USAR AS MESMAS CONSTANTES DA SUA GALÁXIA
    const float RAIO_NEBULOSA = 1500.0f; // Igual ao raio das estrelas
    const int BRACOS_NEB = 5;            // Igual aos braços das estrelas
    
    for(int i=0; i < total_nebulosas; i++) {
        Nebulosa n;
        
        // --- CÁLCULO DA POSIÇÃO (Igualzinho ao das Estrelas) ---
        int braco = i % BRACOS_NEB;
        
        // Distribuição linear (sem powf) para cobrir o braço todo uniformemente
        float distPercent = (float)GetRandomValue(1000, 10000) / 10000.0f; 
        float distancia = distPercent * RAIO_NEBULOSA;

        float anguloBase = (float)braco * ((2.0f * PI) / BRACOS_NEB);
        float torcao = distancia * 0.0010f; // MESMA torção das estrelas
        
        // Espalhamento leve para a nebulosa ficar "em cima" do braço
        float espalhamento = GetRandomValue(-200, 200); 

        float anguloFinal = anguloBase + torcao;
        n.pos.x = cosf(anguloFinal) * distancia + espalhamento;
        n.pos.y = sinf(anguloFinal) * distancia + espalhamento;

        // --- VISUAL ---
        // Cores HSV aleatórias (bonitas e variadas)
        n.cor_base = ColorFromHSV((float)GetRandomValue(0, 360), 0.6f, 0.5f);
        
        int parts = 100; 
        for(int p=0; p<parts; p++) {
            n.offsets_particulas.push_back({(float)GetRandomValue(-500, 300), (float)GetRandomValue(-200, 800)});
            n.tamanhos.push_back((float)GetRandomValue(40, 200));
            n.alphas.push_back(GetRandomValue(5, 20));
        }
        
        nebulosas.push_back(n);
    }*/

    // ==================================================================
    // ==================================================================
    // GERAÇÃO CONTROLADA POR MAPA DE LÓGICA (RGB)
    // ==================================================================
    galaxia.clear();
    
    // 1. CARREGAR IMAGENS
    Image imgLogica = LoadImage("mapa_logica.png"); // Agora tem Verde, Vermelho e Branco
    Image imgVisual = LoadImage("mapa_visual.png"); // Só serve para virar textura de fundo

    // Validação
    if (imgLogica.width == 0 || imgVisual.width == 0) {
        imgLogica = GenImageColor(1200, 700, BLACK);
        imgVisual = GenImageColor(1200, 700, BLACK);
    }

    // 2. PREPARAR O FUNDO (Visual)
    Texture2D texFundoGalaxia = LoadTextureFromImage(imgVisual);

    // 3. LOOP DE GERAÇÃO (Lendo apenas a Lógica)
    const int TENTATIVAS = 15000; // Aumentei tentativas para preencher bem os braços verdes
    int estrelasCriadas = 0;
    const int LIMITE_ESTRELAS = 4000; // Quantidade de estrelas desejada

    for (int i = 0; i < TENTATIVAS; i++) {
        if (estrelasCriadas >= LIMITE_ESTRELAS) break;

        Estrela nova_estrela;
        bool spawnar = false;

        // Sorteia pixel
        int xImg = GetRandomValue(0, 1199);
        int yImg = GetRandomValue(0, 699);

        // Define posição no mundo (Escala 5x)
        nova_estrela.pos.x = (xImg * 5.0f) - 3000.0f;
        nova_estrela.pos.y = (yImg * 5.0f) - 1750.0f;

        // LER APENAS O PIXEL DE LÓGICA
        Color pLogica = GetImageColor(imgLogica, xImg, yImg);

        // --- REGRA 1: DEUSES (Vermelho Puro) ---
        if (pLogica.r > 200 && pLogica.g < 50 && pLogica.b < 50) { 
            spawnar = true;
            ConfigurarBoss(nova_estrela); 
            
            // Stats
            nova_estrela.nivel_maximo = GetRandomValue(2000001, 2500000);
            nova_estrela.diametro_maximo = (float)GetRandomValue(500, 700);
            nova_estrela.nome_chefe = "DEUS " + GerarNomeProcedural();
            
            // --- CORREÇÃO: RAIOS PARA TIER 5 ---
            nova_estrela.raios.clear(); // Limpa lixo anterior
            for(int k=0; k<8; k++) { // 8 Raios
                Raio r;
                // Escala 1.0f (Normal), mas complexidade alta (6-10 segmentos)
                RegenerarRaio(r, nova_estrela.diametro_maximo/2.0f, 6, 10, 1.0f);
                nova_estrela.raios.push_back(r);
            }

            // Partículas normais
            nova_estrela.particulas_ki.clear();
            int qtd_part = 250; 
            for(int k=0; k<qtd_part; k++) {
                ParticulaAura p;
                p.vida = 0; p.offset = {0,0}; p.velocidade = {0,0}; p.cor = WHITE; p.negativa = false;
                p.tam = (float)GetRandomValue(30, 60) / 10.0f;
                nova_estrela.particulas_ki.push_back(p);
            }
        }
        // --- REGRA 2: ENTIDADES (Branco Puro) ---
        else if (pLogica.r > 200 && pLogica.g > 200 && pLogica.b > 200) {
            spawnar = true;
            ConfigurarBoss(nova_estrela);
            
            // Stats
            nova_estrela.nivel_maximo = GetRandomValue(2500001, 3000000);
            nova_estrela.diametro_maximo = (float)GetRandomValue(700, 1000);
            nova_estrela.nome_chefe = "ENTIDADE " + GerarNomeProcedural();

            // --- CORREÇÃO: RAIOS LONGOS PARA TIER 6 ---
            nova_estrela.raios.clear();
            for(int k=0; k<12; k++) { // 12 Raios (Mais denso)
                Raio r;
                // ESCALA DE COMPRIMENTO: 3.0x a 6.0x maior que o normal
                float escala_longa = (float)GetRandomValue(30, 60) / 10.0f;
                
                // Segmentos altos (8-12) e escala longa
                RegenerarRaio(r, nova_estrela.diametro_maximo/2.0f, 8, 12, escala_longa);
                nova_estrela.raios.push_back(r);
            }

            // --- CORREÇÃO: MUITAS PARTÍCULAS ---
            nova_estrela.particulas_ki.clear();
            int qtd_part = 600; // Aumentei de ~300 para 600 fixo!
            
            for(int k=0; k<qtd_part; k++) {
                ParticulaAura p;
                p.vida = 0; p.offset = {0,0}; p.velocidade = {0,0}; p.cor = WHITE; p.negativa = false;
                p.tam = (float)GetRandomValue(30, 60) / 10.0f;
                nova_estrela.particulas_ki.push_back(p);
            }
        }
        // --- REGRA 3: ESTRELAS COMUNS (Verde #00FF42) ---
        // O Hex #00FF42 equivale a RGB(0, 255, 66)
        // Lógica: Muito Verde (>200), Pouco Vermelho (<100)
        else if (pLogica.g > 200 && pLogica.r < 100) {
            spawnar = true;
            ConfigurarBoss(nova_estrela); // Roda a roleta normal (maioria fraca, alguns bosses médios)
            
            // TRAVA DE SEGURANÇA:
            // Se cair num braço verde, NÃO pode ser Deus nem Entidade (Tier 5 ou 6).
            // Se a sorte gerou um muito forte, rebaixa para Tier 4 (General).
            if (nova_estrela.nivel_maximo > 2000000) {
                nova_estrela.nivel_maximo = GetRandomValue(1000000, 1500000);
                nova_estrela.diametro_maximo = (float)GetRandomValue(250, 400);
                nova_estrela.eh_lendario = false;
                nova_estrela.nome_chefe = "GENERAL " + GerarNomeProcedural();
            }
        }

        if (spawnar) {
            // Animação básica
            nova_estrela.tam_base = GetRandomValue(5, 9);
            nova_estrela.tam_nucleo = nova_estrela.tam_base;
            nova_estrela.crescendo = true;
            nova_estrela.limite_anim = GetRandomValue(5, 15);
            nova_estrela.timer_anim = 0;
            nova_estrela.alcance_cresc = 3;
            
            // Checagem básica de colisão para não empilhar demais nos braços
            bool colidiu = false;
            for(const auto& e : galaxia) {
                // Distância simples (sem raiz quadrada pra ser rápido)
                float dx = e.pos.x - nova_estrela.pos.x;
                float dy = e.pos.y - nova_estrela.pos.y;
                if ((dx*dx + dy*dy) < (40*40)) { // 40px de distancia mínima
                    colidiu = true; 
                    break; 
                }
            }

            if (!colidiu) {
                galaxia.push_back(nova_estrela);
                estrelasCriadas++;
            }
        }
    }

    // Limpa a memória das imagens (A textura de fundo continua na GPU)
    UnloadImage(imgLogica);
    UnloadImage(imgVisual);

    // --- INICIALIZAÇÃO DO PLAYER ---
    Player* jogador = new Player("Kreits"); // Instancia o Player e a Nave
    int estrelaAtualPlayer = -1;
    int estrelaCasaPlayer = estrelaAtualPlayer;
    float timerPingPlayer = 0.0f; 
    float timerPingCasa = 0.0f;

    // --- NAVEGAÇÃO RÁPIDA (SPORE) ---
    Vector2 posNaveAtual = {0, 0};
    bool animandoViagem = false;
    int estrelaDestinoCurto = -1;
    
    // --- CONTROLE VISUAL DE KI DO JOGADOR NO MAPA ---
    EstadoKi playerEstadoKi = ESCONDIDO;
    float playerEscalaMinima = (float)jogador->pdlBase / jogador->pdlMaximo;
    float playerEscalaAtual = playerEscalaMinima;
    float playerDiametroMax = 800.0f;

    std::vector<Raio> playerRaios(12);
    for(auto& r : playerRaios) RegenerarRaio(r, playerDiametroMax/2.0f, 8, 12, 4.0f);
    
    std::vector<ParticulaAura> playerParticulas(600);
    for(auto& p : playerParticulas) {
        p.vida = 0; p.offset = {0,0}; p.velocidade = {0,0}; p.cor = WHITE; p.negativa = false;
        p.tam = (float)GetRandomValue(30, 60) / 10.0f;
    }

    // --- VARIÁVEIS DE ROTA ---
    int destinoTracado = -1; 
    int timerCliqueBotao = 0;
    int indexEstrelaSelecionada = -1;
    int dangerzoneRaio = GetRandomValue(100,1000);

    // CONTROLE DE EVENTOS
    std::vector<Vector2> trechosPerigo; // x = inicio%, y = fim%
    bool mostrarZonas = false;

    // Busca todas as estrelas sem chefe
    std::vector<int> estrelasSeguras;
    for (int i = 0; i < (int)galaxia.size(); i++) {
        if (!galaxia[i].tem_chefe) {
            estrelasSeguras.push_back(i);
        }
    }

    // --- VARIÁVEIS DA CASA ---
    estrelaCasaPlayer = -1; 
    timerPingCasa = 0.0f; 

    // Sorteia uma delas para ser o planeta natal
    if (!estrelasSeguras.empty()) {
        estrelaAtualPlayer = estrelasSeguras[GetRandomValue(0, estrelasSeguras.size() - 1)];
        
        // AGORA SIM! Salva a casa exata depois que o sorteio foi feito
        estrelaCasaPlayer = estrelaAtualPlayer; 
        
        // Bônus: Já centraliza a câmera onde o player nasceu!
        camera.target = galaxia[estrelaAtualPlayer].pos;
    }

    GerarSistemaEstelar(galaxia[estrelaAtualPlayer]);
    
    // Variáveis de controle
    Estrela* estrelaFocada = nullptr; // Ponteiro para saber qual estrela o mouse está em cima

    // --- CARREGAMENTO AUTOMÁTICO DE TEXTURAS DE FUNDO ---
    std::vector<Texture2D> bgTextures;
    
    int indexGalaxia = 2; // Começa procurando pela galaxy2.png
    
    while (true) {
        std::string filename = "galaxy" + std::to_string(indexGalaxia) + ".png";
        
        // O Truque: Se o arquivo não existe, para de procurar e sai do loop
        if (!FileExists(filename.c_str())) break; 
        
        Texture2D tex = LoadTexture(filename.c_str());
        bgTextures.push_back(tex);
        TraceLog(LOG_INFO, "Galaxia de fundo carregada: %s", filename.c_str());
        
        indexGalaxia++; // Próxima (3, 4, 5...)
    }

    // Se não achou nenhuma (esqueceu de por os arquivos), avisa
    if (bgTextures.empty()) {
         TraceLog(LOG_WARNING, "Nenhuma galaxia de fundo encontrada (galaxy2.png, etc)!");
    }

    std::vector<BackgroundGalaxy> bgGalaxies;
    const int QTD_BG_GALAXIES = 40; // Quantidade de galáxias no fundo

    for (int i = 0; i < QTD_BG_GALAXIES; i++) {
        BackgroundGalaxy bg;
        
        // SPAWN LONGE: Gera uma distância entre 4000 e 8000 do centro
        float angle = GetRandomValue(0, 360) * DEG2RAD;
        float dist = GetRandomValue(4000, 8000); 
        bg.pos = { cosf(angle) * dist, sinf(angle) * dist };

        bg.rotation = GetRandomValue(0, 360);
        // Velocidade BEM lenta (entre 0.01 e 0.03), sentido aleatório (+ ou -)
        bg.rotSpeed = (GetRandomValue(0, 1) == 0 ? 1 : -1) * (GetRandomValue(1, 3) / 100.0f);

        bg.textureIndex = GetRandomValue(0, bgTextures.size() - 1);
        bg.scale = GetRandomValue(40, 120) / 100.0f; // Tamanho varia de 0.4x a 1.2x
        
        bg.pulseOffset = GetRandomValue(0, 100) / 10.0f; // Começa pulso em ponto diferente
        bg.pulseSpeed = GetRandomValue(1, 3) / 5.0f; // Velocidade do pulso
        
        bgGalaxies.push_back(bg);
    }

    // --- GERAÇÃO DE ESTRELAS BRILHANTES (DECORATIVAS) ---
    std::vector<EstrelaBrilhante> shinyStars;
    const int QTD_SHINY = 1000; // Pode aumentar se seu PC aguentar

    for (int i = 0; i < QTD_SHINY; i++) {
        EstrelaBrilhante s;
        
        // Espalha por todo o mapa (-3000 a +3000)
        s.pos.x = (float)GetRandomValue(-3000, 3000);
        s.pos.y = (float)GetRandomValue(-3000, 3000);
        
        // Tamanho variado (pequenas agulhas de luz até brilhos maiores)
        s.tamanho = (float)GetRandomValue(5, 15) / 10.0f; // 0.5 a 1.5
        
        s.alphaBase = (float)GetRandomValue(4, 8) / 10.0f; // 0.4 a 0.8
        s.velocidadePisca = (float)GetRandomValue(2, 5);
        s.offsetPisca = (float)GetRandomValue(0, 100);
        
        // Cores levemente variadas (Ciano, Branco, Amarelo pálido)
        int corTipo = GetRandomValue(0, 5);
        if (corTipo == 0) s.cor = SKYBLUE;
        else if (corTipo == 1) s.cor = { 200, 200, 255, 255 }; // Azulado
        else if (corTipo == 2) s.cor = { 255, 255, 200, 255 }; // Amarelado
        else s.cor = WHITE;
        
        shinyStars.push_back(s);
    }

    // --- GERAÇÃO DE ZONAS DE METEOROS (DANGER ZONES FIXAS) ---
    std::vector<ZonaMeteoro> zonasMeteoros;
    const int QTD_ZONAS = 30; // Ajuste a quantidade de áreas perigosas
    
    for (int i = 0; i < QTD_ZONAS; i++) {
        ZonaMeteoro z;
        z.pos.x = (float)GetRandomValue(-2400, 2400);
        z.pos.y = (float)GetRandomValue(-1600, 1600);
        z.raio = (float)GetRandomValue(150, 450); // Tamanhos variados (15 a 45 Anos-Luz)
        zonasMeteoros.push_back(z);
    }

    while (!WindowShouldClose()) {
        
        // =============================================================
        // --- INPUT & CÂMERA (CONTROLES DE MAPA TÁTICO) ---
        // =============================================================
        
        // 0. ANCORA DEFINITIVA (Impede o mouse de mover a tela sozinho)
        camera.offset = { screenWidth / 2.0f, screenHeight / 2.0f };

        // 1. ZOOM (Scroll + Teclado)
        float wheel = GetMouseWheelMove();
        if (IsKeyDown(KEY_UP)) wheel = 1.0f;
        if (IsKeyDown(KEY_DOWN)) wheel = -1.0f;

        if (wheel != 0.0f) {
            // Se estiver rastreando algo (Travada), o zoom obrigatoriamente foca no centro.
            // Se estiver livre, o zoom foca onde o mouse está apontando.
            Vector2 mousePos = GetMousePosition();
            if (cameraTravada || IsKeyDown(KEY_UP) || IsKeyDown(KEY_DOWN)) {
                mousePos = { screenWidth / 2.0f, screenHeight / 2.0f };
            }

            Vector2 mouseWorldBefore = GetScreenToWorld2D(mousePos, camera);

            float scaleFactor = 1.0f + (0.05f * fabsf(wheel));
            if (wheel < 0) scaleFactor = 1.0f / scaleFactor;

            float piorCenarioZoom = (float)screenHeight / 6000.0f; 
            float minZoom = piorCenarioZoom * 0.8f; 
            camera.zoom = Clamp(camera.zoom * scaleFactor, minZoom, 3.0f);

            Vector2 mouseWorldAfter = GetScreenToWorld2D(mousePos, camera);

            // SÓ desliza a tela se a câmera estiver LIVRE. 
            // Se estiver travada, o rastreio ali embaixo assume o controle e evita o bug.
            if (!cameraTravada) {
                camera.target.x += (mouseWorldBefore.x - mouseWorldAfter.x);
                camera.target.y += (mouseWorldBefore.y - mouseWorldAfter.y);
            }
        }

        // 2. PAN (WASD) - Move e destrava a câmera instantaneamente
        float moveSpeed = 15.0f / camera.zoom; 
        if (IsKeyDown(KEY_W)) { camera.target.y -= moveSpeed; cameraTravada = false; }
        if (IsKeyDown(KEY_S)) { camera.target.y += moveSpeed; cameraTravada = false; }
        if (IsKeyDown(KEY_A)) { camera.target.x -= moveSpeed; cameraTravada = false; }
        if (IsKeyDown(KEY_D)) { camera.target.x += moveSpeed; cameraTravada = false; }

        // Ping do Player (Tecla P) - Trava a câmera de volta na Nave
        if (IsKeyPressed(KEY_P)) { 
            timerPingPlayer = 3.0f; // Ativa o sinalizador por 3 segundos
            cameraTravada = true;
            focoCamera = -1; // -1 = Nave do Player
        }

        // --- INPUT CASA (Tecla H) ---
        if (IsKeyPressed(KEY_H)) { 
            timerPingCasa = 3.0f; // Ativa o radar da casa por 3 segundos
        }

        // Atualização dos timers
        if (timerPingPlayer > 0) timerPingPlayer -= GetFrameTime();
        if (timerPingCasa > 0) timerPingCasa -= GetFrameTime(); 

        // --- SISTEMA DE RASTREIO ROTACIONAL ---
        if (cameraTravada) {
            float piorCenarioZoom = (float)screenHeight / 6000.0f; 
            float minZoom = piorCenarioZoom * 0.8f; 
            
            // Só rastreia e move a tela se o jogador deu zoom in (não está vendo a galáxia toda)
            if (camera.zoom > minZoom + 0.1f) {
                if (focoCamera == -1) {
                    camera.target = posNaveAtual; // Gruda e segue o jogador
                } else if (focoCamera >= 0 && focoCamera < galaxia.size()) {
                    camera.target = galaxia[focoCamera].pos; // Gruda e segue a estrela clicada
                }
            }
        }
        
        // 3. CLAMP (PRENDER A CÂMERA NAS BORDAS)
        float worldScreenW = screenWidth / camera.zoom;
        float worldScreenH = screenHeight / camera.zoom;
        const float LIMIT_MUNDO = 6000.0f; 

        float minX = -(LIMIT_MUNDO / 2.0f) + (worldScreenW / 2.0f);
        float maxX =  (LIMIT_MUNDO / 2.0f) - (worldScreenW / 2.0f);
        float minY = -(LIMIT_MUNDO / 2.0f) + (worldScreenH / 2.0f);
        float maxY =  (LIMIT_MUNDO / 2.0f) - (worldScreenH / 2.0f);

        if (minX > maxX) camera.target.x = 0.0f;
        else camera.target.x = Clamp(camera.target.x, minX, maxX);

        if (minY > maxY) camera.target.y = 0.0f;
        else camera.target.y = Clamp(camera.target.y, minY, maxY);

       // Detecção do Mouse (SCREEN TO WORLD)
        Vector2 mouseScreen = GetMousePosition();
        Vector2 mouseWorld = GetScreenToWorld2D(mouseScreen, camera);
        
        estrelaFocada = nullptr;
        int indexEstrelaFocada = -1; 
        
        for (int i = 0; i < galaxia.size(); i++) {
            float raioClick = galaxia[i].tam_nucleo + 30.0f; 
            
            if (CheckCollisionPointCircle(mouseWorld, galaxia[i].pos, raioClick)) {
                estrelaFocada = &galaxia[i];
                indexEstrelaFocada = i; 
                break;
            }
        }

        // --- SISTEMA DE CLIQUE E SELEÇÃO ---
            // Primeiro verificamos se o mouse está em cima da UI para não deselecionar a estrela ao clicar no botão
            bool mouseNaUI = false;
            if (indexEstrelaSelecionada != -1) {
                Vector2 screenPos = GetWorldToScreen2D(galaxia[indexEstrelaSelecionada].pos, camera);
                int boxW = 260; int boxH = 140;
                if (indexEstrelaSelecionada == estrelaAtualPlayer) {
                    boxH = 170 + (galaxia[indexEstrelaSelecionada].planetas.size() * 15);
                } else {
                    boxH = 140; 
                }
                int boxX = screenPos.x + 30; int boxY = screenPos.y - 60;
                if (boxX + boxW > screenWidth) boxX = screenPos.x - boxW - 30;
                if (boxY + boxH > screenHeight) boxY = screenPos.y - boxH;
                if (boxY < 0) boxY = 10;
                Rectangle boxRect = { (float)boxX, (float)boxY, (float)boxW, (float)boxH };
                
                if (CheckCollisionPointRec(mouseScreen, boxRect)) mouseNaUI = true;
            }
            
            // Impede que clicar no botão "VISUAL: ON/OFF" feche a janela da estrela
            Rectangle btnVisual = { (float)screenWidth - 140, 20, 120, 30 };
            if (CheckCollisionPointRec(mouseScreen, btnVisual)) mouseNaUI = true;

            // Se clicou com o botão esquerdo e não foi em cima de nenhum botão/menu...
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !mouseNaUI) {
                if (indexEstrelaFocada != -1) {
                    indexEstrelaSelecionada = indexEstrelaFocada; // Seleciona a nova estrela
                    focoCamera = indexEstrelaFocada; 
                    cameraTravada = true;
                } else {
                    indexEstrelaSelecionada = -1; // Clicou no vazio, deseleciona e fecha a aba
                }
            }

            // =============================================================
        // UI FIXA (SCREEN SPACE)
        // =============================================================
        

        // --- UPDATE GERAL (FÍSICA, LÓGICA DE TIERS E EFEITOS) ---
        // --- VARIÁVEL DE CONTROLE DE FLUXO (THROTTLING) ---
        // Declaramos static para ela persistir entre os frames sem resetar
        // Isso impede que 100 inimigos decidam brilhar no mesmo frame
        static int timer_fluxo_ki = 0;
        if (timer_fluxo_ki > 0) timer_fluxo_ki--;


        // --- INPUT E LÓGICA DO KI DO JOGADOR 
        if (estrelaAtualPlayer >= 0) {
            // Velocidade do dimer
            float velocidadeDimer = 0.006f; 

            // Se segura o C, sobe o Ki
            if (IsKeyDown(KEY_C)) {
                playerEscalaAtual += velocidadeDimer;
                if (playerEscalaAtual > 1.0f) playerEscalaAtual = 1.0f;
                playerEstadoKi = ELEVANDO;
            } 
            // Se segura o Z, abaixa o Ki
            else if (IsKeyDown(KEY_Z)) {
                playerEscalaAtual -= velocidadeDimer;
                if (playerEscalaAtual < playerEscalaMinima) playerEscalaAtual = playerEscalaMinima;
                playerEstadoKi = SUPRIMINDO;
            } 
            // Se não está apertando nada, estabiliza onde parou!
            else {
                if (playerEscalaAtual <= playerEscalaMinima) {
                    playerEstadoKi = ESCONDIDO;
                } else {
                    playerEstadoKi = MAXIMO; // Aqui "MAXIMO" significa apenas que está estabilizado
                    
                    // Pequeno efeito de pulsação APENAS se o Ki estiver no talo (100%)
                    if (playerEscalaAtual >= 1.0f) {
                        if(GetRandomValue(0,10) > 8) playerEscalaAtual = 1.01f; 
                        else playerEscalaAtual = 1.0f;
                    }
                }
            }

            // Interpolação Matemática do Nível de Poder (Atualiza o número em tempo real)
            float rangeP = 1.0f - playerEscalaMinima;
            float pctP = (playerEscalaAtual - playerEscalaMinima) / rangeP;
            if (pctP < 0) pctP = 0; if (pctP > 1) pctP = 1;
            
            jogador->pdlAtual = jogador->pdlBase + (int)((jogador->pdlMaximo - jogador->pdlBase) * pctP);

            // Animação de Partículas e Raios
            if (playerEscalaAtual > playerEscalaMinima + 0.01f) {
                // O tamanho físico da aura continua proporcional ao PDL
                float raio_visual = (float)jogador->pdlAtual / 2000.0f;
                if (raio_visual < 15) raio_visual = 15;

                // Define as propriedades dos raios baseadas estritamente no PDL ATUAL
                float espessuraRaio = 1.0f;
                int minSeg = 3; int maxSeg = 6; // Curtos e retos por padrão
                int chanceGerar = 5;            // Raros por padrão
                
                // Tier 1: Suprimido (até 150k) -> Raios quase invisíveis
                if (jogador->pdlAtual < 150000) {
                    espessuraRaio = 1.0f; minSeg = 2; maxSeg = 4; chanceGerar = 2;
                }
                // Tier 2: Guerreiro Z (até 600k) -> Raios finos, médios
                else if (jogador->pdlAtual < 600000) {
                    espessuraRaio = 1.5f; minSeg = 3; maxSeg = 6; chanceGerar = 8;
                }
                // Tier 3: General (até 1.5M) -> Raios grossos, densos
                else if (jogador->pdlAtual < 1500000) {
                    espessuraRaio = 3.0f; minSeg = 5; maxSeg = 10; chanceGerar = 20;
                }
                // Tier 4: Entidade/Deus (Acima de 1.5M) -> Caos Total
                else {
                    espessuraRaio = 5.0f; minSeg = 8; maxSeg = 15; chanceGerar = 40;
                }

                // Atualiza os raios usando os parâmetros do Tier
                for (auto& r : playerRaios) {
                    r.timer_troca--;
                    // Só regenera se o timer acabou E passar na chance do Tier
                    if (r.timer_troca <= 0 && GetRandomValue(0, 100) < chanceGerar) {
                        // O tamanho do raio segue o raio_visual da aura
                        RegenerarRaio(r, raio_visual, minSeg, maxSeg, espessuraRaio);
                    }
                    // Se não gerou, apaga o raio antigo para não ficar flutuando
                    else if (r.timer_troca <= 0) {
                        r.pontos.clear();
                        r.timer_troca = GetRandomValue(10, 30); // Espera um pouco pra tentar de novo
                    }
                }
            }
            float raio_visual_particulas = (float)jogador->pdlAtual / 2000.0f;
            if (raio_visual_particulas < 15) raio_visual_particulas = 15;

            int particulasAlvo = (jogador->pdlAtual - 50000) / 5000; 
            if (particulasAlvo < 0) particulasAlvo = 0;
            if (particulasAlvo > 600) particulasAlvo = 600;

            int chanceNegativa = 0;
            if (jogador->pdlAtual > 1500000) {
                chanceNegativa = (jogador->pdlAtual - 1500000) / 30000; 
                if (chanceNegativa > 50) chanceNegativa = 50; 
            }

            int indexParticula = 0;
            for (auto& p : playerParticulas) {
                // 1. ATUALIZA AS VIVAS
                if (p.vida > 0.0f) {
                    p.offset.x += p.velocidade.x; 
                    p.offset.y += p.velocidade.y;
                    float distSq = (p.offset.x*p.offset.x) + (p.offset.y*p.offset.y);
                    
                    if (p.negativa) { 
                        if (distSq < 100.0f) p.vida = 0.0f; 
                    } else { 
                        p.vida -= 0.015f; 
                        if (p.vida <= 0.0f || distSq > (raio_visual_particulas * raio_visual_particulas * 2.0f)) p.vida = 0.0f; 
                    }
                }
                
                // 2. GERA NOVAS
                if (p.vida <= 0.0f && indexParticula < particulasAlvo) {
                    p.vida = 1.0f; 
                    if (GetRandomValue(0, 100) < chanceNegativa) { 
                        p.negativa = true; 
                        p.cor = BLACK;
                        float ang = GetRandomValue(0, 360) * DEG2RAD;
                        float raioSpawn = raio_visual_particulas * 1.0f; 
                        
                        p.offset = { cosf(ang) * raioSpawn, sinf(ang) * raioSpawn };
                        float vel = GetRandomValue(40, 80) / 10.0f;
                        p.velocidade = { -cosf(ang) * vel, -sinf(ang) * vel };
                        p.tam = GetRandomValue(30, 60) / 10.0f;
                    } else {
                        p.negativa = false;
                        p.offset = { (float)GetRandomValue(-10, 10), (float)GetRandomValue(-10, 10) };
                        float ang = GetRandomValue(0, 360) * DEG2RAD;
                        float vel = GetRandomValue(25, 55) / 10.0f; 
                        p.velocidade = { cosf(ang) * vel, sinf(ang) * vel };
                        p.cor = GetRandomValue(0, 1) ? WHITE : jogador->corAura;
                        p.tam = GetRandomValue(30, 70) / 10.0f;
                    }
                }
                indexParticula++;
            }
        }

        // --- LÓGICA DE MOVIMENTO RÁPIDO ---
        if (animandoViagem) {
            Vector2 alvo = galaxia[estrelaDestinoCurto].pos;
            float dx = alvo.x - posNaveAtual.x;
            float dy = alvo.y - posNaveAtual.y;
            float dist = sqrt(dx*dx + dy*dy);
            
            if (dist < 5.0f) { // Chegou ao destino
                estrelaAtualPlayer = estrelaDestinoCurto;
                posNaveAtual = alvo;
                animandoViagem = false;
                GerarSistemaEstelar(galaxia[estrelaAtualPlayer]);
            } else {
                // Desliza suavemente (Ajuste o 1500.0f se quiser mais rápido ou devagar)
                float velNave = 25.0f * GetFrameTime(); 
                posNaveAtual.x += (dx / dist) * velNave;
                posNaveAtual.y += (dy / dist) * velNave;
            }
        } else if (estrelaAtualPlayer >= 0) {
            // Se não está viajando, fica ancorado na estrela atual
            posNaveAtual = galaxia[estrelaAtualPlayer].pos;
        }

        // --- UPDATE GERAL ---
        
        // ROTAÇÃO DAS ZONAS DE PERIGO (Acompanham a gravidade da galáxia)
        for (auto& z : zonasMeteoros) {
            RotacionarPonto(z.pos, -0.009f); // Exatamente a mesma velocidade das estrelas
        }

        for (int i = 0; i < galaxia.size(); i++) {
            Estrela& e = galaxia[i]; 

            // 1. ROTAÇÃO
            RotacionarPonto(e.pos, -0.009f); 

            // 2. LÓGICA DO KI
            if (e.tem_chefe) {
                
                // Calcula tempos baseados no Tier (Mantido da sua lógica anterior)
                int tempo_sup_min = 10 * 60; int tempo_sup_max = 30 * 60;
                int tempo_ki_min = 5 * 60;   int tempo_ki_max = 15 * 60;

                if (e.nivel_maximo > 500000 && e.nivel_maximo <= 1000000) { // Tier 2
                    tempo_sup_min = 20*60; tempo_sup_max = 40*60; tempo_ki_min = 10*60; tempo_ki_max = 20*60;
                } else if (e.nivel_maximo > 1000000 && e.nivel_maximo <= 1500000) { // Tier 3
                    tempo_sup_min = 30*60; tempo_sup_max = 40*60; tempo_ki_min = 15*60; tempo_ki_max = 25*60;
                } else if (e.nivel_maximo > 1500000 && e.nivel_maximo <= 2000000) { // Tier 4
                    tempo_sup_min = 40*60; tempo_sup_max = 50*60; tempo_ki_min = 20*60; tempo_ki_max = 30*60;
                } else if (e.nivel_maximo > 2000000 && e.nivel_maximo <= 2500000) { // Tier 5
                    tempo_sup_min = 50*60; tempo_sup_max = 60*60; tempo_ki_min = 25*60; tempo_ki_max = 35*60;
                } else if (e.nivel_maximo > 2500000) { // Tier 6
                    tempo_sup_min = 60*60; tempo_sup_max = 90*60; tempo_ki_min = 30*60; tempo_ki_max = 40*60;
                }

                // Atualiza visual
                float range = 1.0f - e.escala_minima;
                if (range <= 0.0001f) range = 0.0001f;
                float pct = (e.escala_atual - e.escala_minima) / range;
                if (pct < 0) pct = 0; if (pct > 1) pct = 1;
                e.nivel_atual = e.nivel_base + (int)((e.nivel_maximo - e.nivel_base) * pct);

                switch (e.estado_ki) {
                    case ESCONDIDO:
                        e.escala_atual = e.escala_minima;
                        e.timer_estado--;
                        
                        // --- FILA PARA PERFORMANCE ---
                        if (e.timer_estado <= 0) {
                            // Só permite subir se a fila estiver livre
                            if (timer_fluxo_ki <= 0) {
                                e.estado_ki = ELEVANDO;
                                
                                // Ocupa a fila por um tempo aleatório (0.3s a 0.8s)
                                // Isso garante que os inimigos liguem UM POR UM, nunca todos juntos.
                                timer_fluxo_ki = GetRandomValue(100, 300); 
                            } 
                            else {
                                // Se a fila está cheia, espera mais um pouquinho (0.5s) e tenta de novo
                                e.timer_estado = 100; 
                            }
                        }
                        break;

                    case ELEVANDO:
                        e.escala_atual += 0.005f; 
                        if (e.escala_atual >= 1.0f) {
                            e.escala_atual = 1.0f; e.estado_ki = MAXIMO;
                            e.timer_estado = GetRandomValue(tempo_ki_min, tempo_ki_max); 
                        }
                        break;

                    case MAXIMO:
                        e.escala_atual = 1.0f;
                        if(GetRandomValue(0,10) > 8) e.escala_atual = 1.02f;
                        e.timer_estado--;
                        if (e.timer_estado <= 0) e.estado_ki = SUPRIMINDO;
                        break;

                    case SUPRIMINDO:
                        e.escala_atual -= 0.008f; 
                        if (e.escala_atual <= e.escala_minima) {
                            e.escala_atual = e.escala_minima; e.estado_ki = ESCONDIDO;
                            e.timer_estado = GetRandomValue(tempo_sup_min, tempo_sup_max); 
                        }
                        break;
                }
                
                // 3. EFEITOS (Só processa se estiver visível)
                // Adicionei uma checagem extra de Culling aqui para performance máxima
                // Se a estrela estiver fora da tela, NEM CALCULA PARTÍCULAS.
                

                if (e.escala_atual > e.escala_minima + 0.05f) {
                     float raio_visual = (float)e.nivel_atual / 2000.0f;
                     if (raio_visual < 15) raio_visual = 15;

                     // Raios
                     for (auto& r : e.raios) {
                        r.timer_troca--;
                        if (r.timer_troca <= 0) {
                            int seg_min=3, seg_max=5; float escala=1.0f;
                            if(e.nivel_atual > 1500000) { seg_min=6; seg_max=10; }
                            if(e.nivel_atual > 2500000) { seg_min=8; seg_max=12; escala=4.0f; }
                            RegenerarRaio(r, raio_visual, seg_min, seg_max, escala);
                        }
                     }
                     // Partículas
                     if(e.nivel_atual > 500000) {
                         for (auto& p : e.particulas_ki) {
                            p.offset.x += p.velocidade.x; p.offset.y += p.velocidade.y;
                            float distSq = (p.offset.x*p.offset.x) + (p.offset.y*p.offset.y);
                            bool respawn = false;
                            if(p.negativa) { if(distSq < 25.0f) respawn = true; } 
                            else { p.vida -= 0.02f; if(p.vida <= 0 || distSq > (raio_visual*raio_visual)) respawn = true; }

                            if(respawn) {
                                p.vida = 1.0f; p.negativa = false;
                                if(e.nivel_atual > 2000000 && GetRandomValue(0,100)<40) { 
                                    p.negativa = true; p.cor = BLACK;
                                    float ang = GetRandomValue(0,360)*DEG2RAD;
                                    float rSpawn = raio_visual * (GetRandomValue(90, 110)/100.0f);
                                    p.offset = {cosf(ang)*rSpawn, sinf(ang)*rSpawn};
                                    float vel = GetRandomValue(30, 60) / 10.0f;
                                    p.velocidade = {-cosf(ang)*vel, -sinf(ang)*vel};
                                    p.tam = GetRandomValue(30,50)/10.0f;
                                } else { 
                                    p.offset = {(float)GetRandomValue(-5, 5), (float)GetRandomValue(-5, 5)};
                                    float ang = GetRandomValue(0,360)*DEG2RAD;
                                    float vel = GetRandomValue(15, 35) / 10.0f; 
                                    p.velocidade = {cosf(ang)*vel, sinf(ang)*vel};
                                    int tipo = GetRandomValue(0, 2);
                                    if (tipo == 0) p.cor = WHITE;
                                    else if (tipo == 1) p.cor = ColorAlpha(e.cor_aura, 0.8f);
                                    else p.cor = ColorAlpha(e.cor_aura, 0.4f);
                                    p.tam = GetRandomValue(30,60)/10.0f;
                                }
                            }
                         }
                     }
                }
            } 

            // 4. ANIMAÇÃO FÍSICA
            e.timer_anim++;
            if(e.timer_anim > e.limite_anim) {
                e.timer_anim = 0;
                if(e.crescendo) { e.tam_nucleo++; if(e.tam_nucleo >= e.tam_base + e.alcance_cresc) e.crescendo = false; } 
                else { e.tam_nucleo--; if(e.tam_nucleo <= e.tam_base - e.alcance_cresc) e.crescendo = true; }
            }
        }

            // --- DRAW ---
        // --- DRAW ---
        BeginDrawing();
        ClearBackground(BLACK);

        // Incrementa o ângulo total para o desenho da textura
        anguloGalaxia -= 0.01f; // MENOS igual, para girar junto com as estrelas
        if (anguloGalaxia < 0.0f) anguloGalaxia += 360.0f;

        BeginMode2D(camera);

            // --- DESENHO DO FUNDO DISTANTE (PARALLAX) ---
            // Deve ser desenhado ANTES da galáxia principal
            if (mostrarVisual) {
                float tempo = GetTime(); // Pega o tempo atual para a pulsação

                for (auto& bg : bgGalaxies) {
                    // 1. Atualiza Rotação
                    bg.rotation += bg.rotSpeed;

                    // 2. Calcula Pulsação (Onda Senoidal suave)
                    // Varia o alpha entre 0.3 (escuro) e 0.8 (brilhante)
                    float pulse = (sinf(tempo * bg.pulseSpeed + bg.pulseOffset) + 1.0f) / 2.0f; // 0.0 a 1.0
                    float alpha = 0.3f + (pulse * 0.5f); 
                    Color pulseColor = ColorAlpha(WHITE, alpha);

                    Texture2D tex = bgTextures[bg.textureIndex];
                    
                    // Define destino e pivô para rotação centralizada
                    Rectangle source = {0, 0, (float)tex.width, (float)tex.height};
                    Rectangle dest = {bg.pos.x, bg.pos.y, tex.width * bg.scale, tex.height * bg.scale};
                    Vector2 origin = {dest.width / 2.0f, dest.height / 2.0f}; // Pivô no centro

                    DrawTexturePro(tex, source, dest, origin, bg.rotation, pulseColor);
                }
            }

            // 1. DESENHAR O FUNDO DA GALÁXIA (A Imagem Visual)
            // A imagem tem 1200x700, mas o mundo tem 6000x3500.
            // Desenhamos ela esticada para cobrir o mundo todo.
            // Posição X: -3000, Y: -1750 (Canto superior esquerdo do mapa)
            // Largura: 6000, Altura: 3500
            
            // Dica: Use uma cor escura (GRAY ou DARKGRAY) para o fundo não ofuscar as auras
            if (mostrarVisual) {
                // AQUI ESTÁ O SEGREDO DO GIRO PERFEITO:
                // Destino: O retângulo cobre o mundo (-3000, -1750)
                Rectangle dest = {0, 0, 6000, 3500};
                
                // Origem (Pivô): O ponto de giro deve ser O MEIO da imagem no mundo
                // Metade de 6000 é 3000. Metade de 3500 é 1750.
                Vector2 origin = {(float)texFundoGalaxia.width/2, (float)texFundoGalaxia.height/2};

                // Agora passamos 'anguloGalaxia' no argumento de rotação
                DrawTexturePro(texFundoGalaxia, 
                               {0, 0, (float)texFundoGalaxia.width, (float)texFundoGalaxia.height}, 
                               dest, 
                               origin, 
                               anguloGalaxia, // <--- ROTAÇÃO AQUI
                               WHITE);
                               
            }

            // --- DESENHAR ZONAS DE METEOROS ---
            if (mostrarZonas) {
                BeginBlendMode(BLEND_ADDITIVE);
                for (const auto& z : zonasMeteoros) {
                    // Gradiente vermelho esfumaçado
                    DrawCircleGradient(z.pos.x, z.pos.y, z.raio, ColorAlpha(RED, 0.85f), BLANK);
                    // Borda sutil
                    DrawCircleLines(z.pos.x, z.pos.y, z.raio, ColorAlpha(RED, 0.05f));
                }
                EndBlendMode();
            }

            // 2. Poeira (Opcional, pode tirar se o fundo já for bonito)
            for(const auto& p : poeira) DrawPixelV(p, {255, 255, 255, 30});

            // =============================================================
            // 2. CAMADA DE LUZ (MODO ADITIVO) - AURA E BRILHOS
            // =============================================================
            BeginBlendMode(BLEND_ADDITIVE); 
            
            for (const auto& e : galaxia) {
                // Só desenha se tiver chefe e Ki visível
                if (e.tem_chefe && e.nivel_atual > 1000) {

                    // A. AURA SUAVE
                    // Calcula raio baseado no poder atual (ajuste o divisor 2000.0f para calibrar tamanho)
                    float raio_base = (float)e.nivel_atual / 2000.0f; 
                    if (raio_base < 15.0f) raio_base = 15.0f;

                    float pulso = sin(GetTime() * 3.0f) * (raio_base * 0.05f);
                    float raio_final = raio_base + pulso;

                    Color cor_glow = ColorAlpha(e.cor_aura, 0.6f); 

                    // Desenha o brilho externo suave
                    DrawCircleGradient(e.pos.x, e.pos.y, raio_final, cor_glow, BLANK);
                    // Desenha o núcleo de energia concentrada
                    DrawCircleGradient(e.pos.x, e.pos.y, raio_final * 0.5f, ColorAlpha(WHITE, 0.5f), BLANK);

                    // B. RAIOS (TIER 3+: > 1 Milhão)
                    if (e.nivel_atual > 1000000) {
                        float espessura = 1.0f;
                        bool grandioso = (e.nivel_atual > 1500000); // Tier 4 e 5: Raios grossos

                        if (grandioso) espessura = 3.0f;

                        for (const auto& r : e.raios) {
                             if (r.pontos.size() > 1) {
                                for (size_t i = 0; i < r.pontos.size() - 1; ++i) {
                                    Vector2 p1 = {e.pos.x + r.pontos[i].x, e.pos.y + r.pontos[i].y};
                                    Vector2 p2 = {e.pos.x + r.pontos[i+1].x, e.pos.y + r.pontos[i+1].y};
                                    
                                    // Raio Colorido
                                    DrawLineEx(p1, p2, espessura, ColorAlpha(e.cor_aura, 0.8f));
                                    // Núcleo Branco do Raio (se for forte)
                                    if (grandioso) DrawLineEx(p1, p2, 1.0f, WHITE);
                                }
                            }
                        }
                    }

                    // C. PARTÍCULAS BRILHANTES (TIER 2+: > 500k)
                    if (e.nivel_atual > 500000) {
                        for (const auto& p : e.particulas_ki) {
                            if (!p.negativa) { // Só desenha as de luz aqui
                                DrawCircleV({e.pos.x + p.offset.x, e.pos.y + p.offset.y}, p.tam, ColorAlpha(p.cor, p.vida));
                                DrawPixelV({e.pos.x + p.offset.x, e.pos.y + p.offset.y}, WHITE); // Brilho extra
                            }
                        }
                    }
                }
            }

            // --- DRAW KI DO JOGADOR (LUZ) ---
            if (estrelaAtualPlayer >= 0) {
                Vector2 posP = posNaveAtual;
                
                // Desenha Aura e Raios apenas se o Ki estiver alto
                if (jogador->pdlAtual > 100000) {
                    float raio_base = (float)jogador->pdlAtual / 2000.0f;
                    float pulso = sin(GetTime() * 3.0f) * (raio_base * 0.05f);
                    float raio_final = raio_base + pulso;

                    DrawCircleGradient(posP.x, posP.y, raio_final, ColorAlpha(jogador->corAura, 0.6f), BLANK);
                    DrawCircleGradient(posP.x, posP.y, raio_final * 0.5f, ColorAlpha(WHITE, 0.5f), BLANK);

                    for (const auto& r : playerRaios) {
                         if (r.pontos.size() > 1) {
                            for (size_t i = 0; i < r.pontos.size() - 1; ++i) {
                                Vector2 p1 = {posP.x + r.pontos[i].x, posP.y + r.pontos[i].y};
                                Vector2 p2 = {posP.x + r.pontos[i+1].x, posP.y + r.pontos[i+1].y};
                                DrawLineEx(p1, p2, r.espessura, ColorAlpha(jogador->corAura, 0.8f));
                                DrawLineEx(p1, p2, r.espessura * 0.4f, WHITE); 
                            }
                        }
                    }
                }

                // Desenha as partículas POSITIVAS de forma independente (Sem trava de PDL!)
                for (const auto& p : playerParticulas) {
                    if (p.vida > 0.0f && !p.negativa) {
                        DrawCircleV({posP.x + p.offset.x, posP.y + p.offset.y}, p.tam, ColorAlpha(p.cor, p.vida));
                        DrawPixelV({posP.x + p.offset.x, posP.y + p.offset.y}, WHITE);
                    }
                }
            }
            EndBlendMode(); // FIM DO MODO ADITIVO

            // =============================================================
            // 3. CAMADA FÍSICA E ESCURA (MODO NORMAL)
            // =============================================================
            for (const auto& e : galaxia) {
                
                // D. PARTÍCULAS NEGATIVAS (TIER 5: > 2 Milhões)
                // Desenhamos aqui fora para serem pretas sólidas em cima da luz
                if (e.tem_chefe && e.nivel_atual > 2000000 && e.nivel_atual < 25000000) {
                    for (const auto& p : e.particulas_ki) {
                        if (p.negativa) {
                            DrawPixelV({e.pos.x + p.offset.x, e.pos.y + p.offset.y}, BLACK);
                            DrawPixelV({e.pos.x+10 + p.offset.x+10, e.pos.y + p.offset.y}, WHITE);
                        }
                    }
                }
                if (e.tem_chefe && e.nivel_atual >= 2500000){
                    for (const auto& p : e.particulas_ki) {
                        if (p.negativa) {
                            DrawCircleV({e.pos.x + p.offset.x, e.pos.y + p.offset.y}, p.tam-2, BLACK);
                            DrawCircleV({e.pos.x+30 + p.offset.x+10, e.pos.y + p.offset.y}, p.tam-2, WHITE);
                        }
                    }
                }

                // E. ESTRELA FÍSICA (NÚCLEO)
                Color corEstrela = WHITE;
                bool estaFocadaOuSelecionada = (&e == estrelaFocada) || (indexEstrelaSelecionada != -1 && &e == &galaxia[indexEstrelaSelecionada]);

                if (estaFocadaOuSelecionada) corEstrela = GREEN;
                
                DrawStarShape(e.pos, e.tam_nucleo, corEstrela);

                // Indicador de seleção
                if (estaFocadaOuSelecionada) {
                     DrawCircleLines(e.pos.x, e.pos.y, e.tam_nucleo + 12.0f, GREEN);
                }

                
            }
            
                // --- DRAW KI DO JOGADOR (PARTÍCULAS NEGATIVAS) ---
            if (estrelaAtualPlayer >= 0) {
                Vector2 posP = posNaveAtual;
                // Desenha de forma independente (Sem trava de PDL!)
                for (const auto& p : playerParticulas) {
                    if (p.vida > 0.0f && p.negativa) {
                        DrawCircleV({posP.x + p.offset.x, posP.y + p.offset.y}, p.tam - 2, BLACK);
                        DrawCircleV({posP.x + 7 + p.offset.x + 9, posP.y + p.offset.y}, p.tam - 2, WHITE);
                    }
                }
            }
            
            // =============================================================
            // 3.5 ROTA TRAÇADA
            // =============================================================
            if (destinoTracado >= 0 && destinoTracado < galaxia.size() && !animandoViagem) {
                Vector2 posOrigem = posNaveAtual;
                Vector2 posDestino = galaxia[destinoTracado].pos;
                
                // DESENHA ROTA VERDE BASE (Cobre tudo)
                BeginBlendMode(BLEND_ADDITIVE);
                DrawLineEx(posOrigem, posDestino, 20.0f, ColorAlpha(GREEN, 0.4f));
                EndBlendMode();
                DrawLineEx(posOrigem, posDestino, 6.0f, GREEN);

                // DESENHA OS TRECHOS DE PERIGO POR CIMA
                for (const auto& trecho : trechosPerigo) {
                    Vector2 inicioPerigo = {
                        posOrigem.x + (posDestino.x - posOrigem.x) * trecho.x,
                        posOrigem.y + (posDestino.y - posOrigem.y) * trecho.x
                    };
                    Vector2 fimPerigo = {
                        posOrigem.x + (posDestino.x - posOrigem.x) * trecho.y,
                        posOrigem.y + (posDestino.y - posOrigem.y) * trecho.y
                    };
                    
                    // Camadas Vermelhas
                    BeginBlendMode(BLEND_ADDITIVE);
                    DrawLineEx(inicioPerigo, fimPerigo, 28.0f, ColorAlpha(RED, 0.6f));       
                    EndBlendMode();
                    DrawLineEx(inicioPerigo, fimPerigo, 10.0f, RED);       
                    
                    Vector2 meioEvento = { (inicioPerigo.x + fimPerigo.x) / 2.0f, (inicioPerigo.y + fimPerigo.y) / 2.0f };
                    
                    // Ícone Alerta
                    float pulso = sin(GetTime() * 15.0f) * 4.0f;
                    BeginBlendMode(BLEND_ADDITIVE);
                    DrawCircleLines(meioEvento.x, meioEvento.y, 20.0f + pulso, ColorAlpha(RED, 0.5f));
                    EndBlendMode();
                    DrawCircleLines(meioEvento.x, meioEvento.y, 16.0f + pulso, RED);
                    int tamFonte = 30 + (int)pulso;
                    int larguraExcl = MeasureText("!", tamFonte);
                    DrawText("!", (int)meioEvento.x - (larguraExcl / 2), (int)meioEvento.y - (tamFonte / 2), tamFonte, WHITE);

                    // Texto DANGER ZONE na fita
                    float dxEv = fimPerigo.x - inicioPerigo.x;
                    float dyEv = fimPerigo.y - inicioPerigo.y;
                    float distEv = sqrt(dxEv*dxEv + dyEv*dyEv);
                    float ang = atan2f(dyEv, dxEv) * RAD2DEG;
                    
                    int fSize = 15;
                    std::string txtBase = "DANGER ZONE == ";
                    int reps = (int)(distEv / MeasureText(txtBase.c_str(), fSize));
                    if (reps < 1) reps = 1;

                    std::string txtFinal = "";
                    for (int i=0; i<reps; i++) txtFinal += txtBase;
                    if (txtFinal.length() > 4) txtFinal = txtFinal.substr(0, txtFinal.length() - 4);

                    float offY = 25.0f; 
                    if (dxEv < 0) { ang += 180.0f; offY = -15.0f; }

                    Vector2 orgTxt = { MeasureText(txtFinal.c_str(), fSize) / 2.0f, offY };
                    DrawTextPro(GetFontDefault(), txtFinal.c_str(), meioEvento, orgTxt, ang, (float)fSize, 2.0f, RED);
                }
                
                // Radar pulsante no destino
                float pulsoRota = sin(GetTime() * 8.0f) * 8.0f;
                DrawCircleLines(posDestino.x, posDestino.y, galaxia[destinoTracado].tam_nucleo + 20.0f + pulsoRota, GREEN);
            }

            // --- DESENHO DO RAIO DE ALCANCE RÁPIDO (10 AL) ---
            if (estrelaAtualPlayer >= 0 && !animandoViagem) {
                DrawCircleLines(posNaveAtual.x, posNaveAtual.y, 100.0f, ColorAlpha(WHITE, 0.2f));
                DrawCircleLines(posNaveAtual.x, posNaveAtual.y, 102.0f, ColorAlpha(SKYBLUE, 0.1f));
            }

            // =============================================================
            // 4. INDICADOR DO JOGADOR
            // =============================================================
            if (estrelaAtualPlayer >= 0 && estrelaAtualPlayer < galaxia.size()) {
                Vector2 posPlayer = posNaveAtual;

                // Matemática do Ping: Multiplicador de tamanho
                float multiplicador = 1.0f;
                if (timerPingPlayer > 0.0f) {
                    multiplicador = 5.0f + sin(GetTime() * 15.0f) * 1.0f; 
                }

                // Animação de flutuação base
                float flutuacao = sin(GetTime() * 4.0f) * 5.0f;
                
                // Medidas dinâmicas
                float largura = 12.0f * multiplicador;
                float altura = 20.0f * multiplicador;
                float baseY = posPlayer.y - (5.0f * multiplicador) + flutuacao;

                // Vértices do triângulo invertido
                Vector2 p1 = { posPlayer.x - largura, baseY - altura }; 
                Vector2 p2 = { posPlayer.x + largura, baseY - altura }; 
                Vector2 p3 = { posPlayer.x, baseY };                    
                
                DrawTriangle(p1, p3, p2, YELLOW); 
                DrawTriangleLines(p1, p3, p2, ORANGE);
                
                int tamanhoFonte = (int)(10 * multiplicador);
                int larguraTexto = MeasureText("PLAYER", tamanhoFonte);
                DrawText("PLAYER", (int)posPlayer.x - (larguraTexto / 2), (int)(baseY - altura - tamanhoFonte - 5), tamanhoFonte, YELLOW);
            }

            // =============================================================
            // 5. INDICADOR DE CASA (HOME)
            // =============================================================
            if (estrelaCasaPlayer >= 0 && estrelaCasaPlayer < galaxia.size()) {
                Vector2 posCasa = galaxia[estrelaCasaPlayer].pos;
                
                // Matemática do Ping da Casa
                float multiplicadorCasa = 1.0f;
                if (timerPingCasa > 0.0f) {
                    multiplicadorCasa = 5.0f + sin(GetTime() * 15.0f) * 1.0f; 
                    
                    // Ondas de radar espaciais ao apertar H
                    BeginBlendMode(BLEND_ADDITIVE);
                    DrawCircleLines(posCasa.x, posCasa.y, 80.0f * multiplicadorCasa, ColorAlpha(SKYBLUE, 0.4f));
                    DrawCircleLines(posCasa.x, posCasa.y, 90.0f * multiplicadorCasa, ColorAlpha(BLUE, 0.2f));
                    EndBlendMode();
                }

                // Animação independente para a casa
                float flutuacaoCasa = sin(GetTime() * 3.0f + 1.0f) * 5.0f;
                
                // Medidas dinâmicas do ícone procedural
                float tamCasa = 16.0f * multiplicadorCasa;
                float baseCasaY = posCasa.y - (10.0f * multiplicadorCasa) + flutuacaoCasa;

                // Fundo suave escuro para garantir que o ícone destaque da estrela
                DrawCircle(posCasa.x, baseCasaY, tamCasa * 1.2f, Fade(BLACK, 0.4f));

                // 1. Quadrado Base (SKYBLUE)
                DrawRectangle(posCasa.x - tamCasa/2, baseCasaY - tamCasa/2, tamCasa, tamCasa, SKYBLUE);
                
                // 2. Telhado (BLUE)
                Vector2 topV = { posCasa.x, baseCasaY - tamCasa };
                Vector2 leftV = { posCasa.x - tamCasa/2 - 2, baseCasaY - tamCasa/2 + 1 };
                Vector2 rightV = { posCasa.x + tamCasa/2 + 2, baseCasaY - tamCasa/2 + 1 };
                DrawTriangle(topV, leftV, rightV, BLUE);
                
                // 3. Porta (DARKBLUE)
                DrawRectangle(posCasa.x + tamCasa/8, baseCasaY, tamCasa/3, tamCasa/2, DARKBLUE);
                
                // Contorno sutil para acabamento
                DrawRectangleLines(posCasa.x - tamCasa/2, baseCasaY - tamCasa/2, tamCasa, tamCasa, Fade(WHITE, 0.3f));

                // Texto HOME
                int tamFonteCasa = (int)(10 * multiplicadorCasa);
                int largTextoCasa = MeasureText("HOME", tamFonteCasa);
                DrawText("HOME", (int)posCasa.x - (largTextoCasa / 2), (int)(baseCasaY - tamCasa - tamFonteCasa - 5), tamFonteCasa, SKYBLUE);
            }

        EndMode2D();

        // --- UI (INTERFACE) ---
        DrawText("MAPA DE NAVEGAÇÃO", 20, 20, 20, WHITE);
        DrawText("WASD: Mover | SCROLL: Zoom", 20, 45, 10, WHITE);
        DrawText("C: ELEVAR PDL", 20, 60, 10, WHITE);
        DrawText("Z: SUPRIMIR PDL", 20, 75, 10, WHITE);
        DrawText("P: MOSTRAR PLAYER", 20, 90, 10, WHITE);
        DrawText("H: MOSTRAR CASA", 20, 105, 10, WHITE);

        // --- BOTÃO TOGGLE VISUAL ---
        // 1. Definir área do botão (Canto Superior Direito)
        btnVisual = { (float)screenWidth - 140, 20, 120, 30 };
        
        // 2. Lógica de Clique (Mouse dentro do retangulo + Clique Esquerdo)
        // Usamos GetMousePosition() direto, pois estamos na UI (sem zoom da camera)
        if (CheckCollisionPointRec(GetMousePosition(), btnVisual)) {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                mostrarVisual = !mostrarVisual; // Inverte o valor (True vira False e vice-versa)
            }
        }

        // 3. Desenho do Botão
        // Muda a cor se estiver Ativo (Verde) ou Inativo (Cinza)
        Color corBtn = mostrarVisual ? DARKGREEN : DARKGRAY;
        
        DrawRectangleRec(btnVisual, corBtn);
        DrawRectangleLinesEx(btnVisual, 2, WHITE); // Borda branca
        
        // Texto do botão
        const char* textoBtn = mostrarVisual ? "VISUAL: ON" : "VISUAL: OFF";
        DrawText(textoBtn, (int)btnVisual.x + 10, (int)btnVisual.y + 8, 10, WHITE);

        // --- BOTÃO ZONAS DE RISCO ---
        Rectangle btnZonas = { (float)screenWidth - 140, 60, 120, 30 }; // Fica 40px abaixo do outro
        if (CheckCollisionPointRec(GetMousePosition(), btnZonas)) {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                mostrarZonas = !mostrarZonas;
            }
        }
        
        Color corBtnZonas = mostrarZonas ? DARKGREEN : DARKGRAY;
        DrawRectangleRec(btnZonas, corBtnZonas);
        DrawRectangleLinesEx(btnZonas, 2, WHITE);
        const char* textoBtnZonas = mostrarZonas ? "ZONAS: ON" : "ZONAS: OFF";
        DrawText(textoBtnZonas, (int)btnZonas.x + 10, (int)btnZonas.y + 8, 10, WHITE);

        // =============================================================
        // 4. INTERFACE DE INFORMAÇÕES (HOVER E SELEÇÃO MULTIPLA)
        // =============================================================
        // Cria uma lista de UIs que precisam aparecer neste frame
        std::vector<int> estrelasParaMostrar;
        
        // 1º Colocamos a Selecionada (Ela é desenhada primeiro, ficando "no fundo" se cruzar)
        if (indexEstrelaSelecionada != -1) {
            estrelasParaMostrar.push_back(indexEstrelaSelecionada);
        }
        
        // 2º Colocamos a do Hover, CASO seja uma estrela diferente da selecionada
        if (indexEstrelaFocada != -1 && indexEstrelaFocada != indexEstrelaSelecionada) {
            estrelasParaMostrar.push_back(indexEstrelaFocada);
        }

        // Loop que desenha as caixinhas
        for (int i = 0; i < (int)estrelasParaMostrar.size(); i++) {
            int indexUI = estrelasParaMostrar[i];
            bool ehSelecionada = (indexUI == indexEstrelaSelecionada);

            Estrela* estrelaUI = &galaxia[indexUI];
            Vector2 screenPos = GetWorldToScreen2D(estrelaUI->pos, camera);
            
            // --- CÁLCULO DA DISTÂNCIA ---
            float dx = estrelaUI->pos.x - galaxia[estrelaAtualPlayer].pos.x;
            float dy = estrelaUI->pos.y - galaxia[estrelaAtualPlayer].pos.y;
            float distMundo = sqrt(dx*dx + dy*dy);
            
            int distanciaAnosLuz = (int)(distMundo / 10.0f);

            int boxW = 260; 
            int boxH = 140; 
            
            // --- CÁLCULO DINÂMICO DA ALTURA DA CAIXA ---
            if (ehSelecionada && indexUI == estrelaAtualPlayer) {
                // Aumenta a caixa original para caber a lista de planetas e dados científicos
                boxH = 170 + (estrelaUI->planetas.size() * 15);
            } else if (ehSelecionada && indexUI != estrelaAtualPlayer) {
                // Altura padrão para as outras estrelas com botões
                boxH = 140; 
            }

            int boxX = screenPos.x + 30; int boxY = screenPos.y - 60;

            if (boxX + boxW > screenWidth) boxX = screenPos.x - boxW - 30; 
            if (boxY + boxH > screenHeight) boxY = screenPos.y - boxH;
            if (boxY < 0) boxY = 10;

            Color borda = estrelaUI->tem_chefe ? estrelaUI->cor_aura : GREEN;
            if (indexUI == estrelaAtualPlayer) borda = YELLOW; // Destaque para a estrela atual

            // DESENHA A CAIXA ORIGINAL (Agora com altura ajustada)
            DrawRectangle(boxX, boxY, boxW, boxH, BLACK);
            DrawRectangle(boxX, boxY, boxW, boxH, Fade(WHITE, 0.1f));
            DrawRectangle(boxX, boxY, boxW, boxH, Fade(GREEN, 0.1f));
            DrawRectangleLines(boxX, boxY, boxW, boxH, borda);

            // Conteúdo Base da Estrela (Sempre aparece)
            if (estrelaUI->tem_chefe) {
                DrawText("SINAL DETECTADO", boxX + 10, boxY + 10, 10, RED);
                DrawText(estrelaUI->nome_chefe.c_str(), boxX + 10, boxY + 25, 20, WHITE);
                
                const char* estadoTexto = "";
                Color corTexto = YELLOW;
                
                if (estrelaUI->estado_ki == ESCONDIDO || estrelaUI->estado_ki == SUPRIMINDO) {
                    estadoTexto = "(Suprimido)"; corTexto = GRAY;
                } else if (estrelaUI->estado_ki == MAXIMO) {
                    estadoTexto = "(MÁXIMO!)"; corTexto = ORANGE;
                } else {
                    estadoTexto = "(Elevando...)";
                }

                DrawText(TextFormat("Ki: %i %s", estrelaUI->nivel_atual, estadoTexto), boxX + 10, boxY + 50, 10, WHITE);
                
                DrawRectangle(boxX + 10, boxY + 70, 240, 6, Fade(GRAY, 0.5f)); 
                float ratio = (float)estrelaUI->nivel_atual / 800000.0f; 
                if (ratio > 1.0f) ratio = 1.0f;
                DrawRectangle(boxX + 10, boxY + 70, (int)(240 * ratio), 6, estrelaUI->cor_aura);
                
                float ratioMax = (float)estrelaUI->nivel_maximo / 800000.0f;
                if (ratioMax > 1.0f) ratioMax = 1.0f;
                if (estrelaUI->estado_ki != MAXIMO) {
                     DrawRectangle((boxX + 10) + (int)(240 * ratioMax), boxY + 65, 2, 16, RED);
                }

                if (estrelaUI->eh_lendario) DrawText("CLASSE: LENDÁRIO", boxX + 140, boxY+10, 10, PURPLE);
                else DrawText("CLASSE: ELITE", boxX + 150, boxY+10, 10, ORANGE);
            }
            else {
                DrawText("SISTEMA SEGURO", boxX + 10, boxY + 10, 10, GREEN);
                DrawText("Setor Desabitado", boxX + 10, boxY + 30, 20, LIGHTGRAY);
                DrawText("Sem sinais de Ki hostil.", boxX + 10, boxY + 60, 10, GRAY);
            }

            // --- LÓGICA DO BOTÃO VS TEXTO DE HOVER ---
            if (ehSelecionada) {
                if (indexUI != estrelaAtualPlayer) {
                    bool dentroDoAlcance = (distanciaAnosLuz <= 10);
                    
                    if (dentroDoAlcance && !animandoViagem) {
                        // --- BOTÃO GO (VIAGEM DIRETA) ---
                        Rectangle btnGo = { (float)boxX + 140, (float)boxY + 85, (float)boxW - 150, 40 };
                        Vector2 mouseScreen = GetMousePosition();
                        bool hoverGo = CheckCollisionPointRec(mouseScreen, btnGo);

                        if (hoverGo && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                            estrelaDestinoCurto = indexUI;
                            animandoViagem = true;
                            destinoTracado = -1; // Cancela rota longa se houver
                        }

                        DrawRectangleRec(btnGo, hoverGo ? GREEN : DARKGREEN);
                        DrawRectangleLinesEx(btnGo, 3, BLACK); 
                        
                        int textW = MeasureText("GO!", 20);
                        DrawText("GO!", (int)(btnGo.x + (btnGo.width / 2) - (textW / 2)), (int)(btnGo.y + 10), 20, BLACK);
                        
                    } else if (!dentroDoAlcance) {
                        // --- BOTÃO TRACAR ROTA (LONGA DISTÂNCIA) ---
                        bool rotaAtiva = (destinoTracado == indexUI);
                        float alturaBtn = rotaAtiva ? 11.0f : 20.0f;
                        float yOffset = rotaAtiva ? 9.0f : 0.0f; 

                        Rectangle btnRota = { (float)boxX + 140, (float)boxY + 85 + yOffset, (float)boxW - 150, alturaBtn+30 };
                        Vector2 mouseScreen = GetMousePosition();
                        bool hoverRota = CheckCollisionPointRec(mouseScreen, btnRota);

                        if (hoverRota && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                            if (rotaAtiva) {
                                destinoTracado = -1; // Desliga a rota
                            } else {
                                destinoTracado = indexUI; 
                                trechosPerigo.clear(); // Limpa a rota anterior
                                
                                Vector2 A = posNaveAtual;
                                Vector2 B = galaxia[destinoTracado].pos;
                                float dx_r = B.x - A.x;
                                float dy_r = B.y - A.y;
                                float distRota = sqrt(dx_r*dx_r + dy_r*dy_r);
                                
                                if (distRota > 0) {
                                    Vector2 dir = { dx_r / distRota, dy_r / distRota }; 
                                    
                                    // 1. Coleta TODOS os trechos que batem em zonas
                                    for (const auto& z : zonasMeteoros) {
                                        Vector2 AC = { z.pos.x - A.x, z.pos.y - A.y };
                                        float t = (AC.x * dir.x) + (AC.y * dir.y); 
                                        Vector2 pontoMaisProximo = { A.x + dir.x * t, A.y + dir.y * t };
                                        
                                        float distCentroSq = (z.pos.x - pontoMaisProximo.x)*(z.pos.x - pontoMaisProximo.x) + 
                                                             (z.pos.y - pontoMaisProximo.y)*(z.pos.y - pontoMaisProximo.y);
                                        
                                        if (distCentroSq < (z.raio * z.raio)) {
                                            float metadeCorda = sqrt((z.raio * z.raio) - distCentroSq); 
                                            float entra = t - metadeCorda;
                                            float sai = t + metadeCorda;
                                            
                                            if (entra < distRota && sai > 0) {
                                                if (entra < 0) entra = 0;
                                                if (sai > distRota) sai = distRota;
                                                trechosPerigo.push_back({ entra / distRota, sai / distRota });
                                            }
                                        }
                                    }

                                    // 2. FUSÃO (Merge) das zonas que estão sobrepostas
                                    if (!trechosPerigo.empty()) {
                                        // Ordena pelo ponto inicial
                                        std::sort(trechosPerigo.begin(), trechosPerigo.end(), [](const Vector2& a, const Vector2& b) {
                                            return a.x < b.x;
                                        });

                                        std::vector<Vector2> mesclados;
                                        mesclados.push_back(trechosPerigo[0]);

                                        for (size_t i = 1; i < trechosPerigo.size(); i++) {
                                            Vector2& ultimo = mesclados.back();
                                            Vector2 atual = trechosPerigo[i];
                                            
                                            if (atual.x <= ultimo.y) { // Se sobrepõe, estica o limite final
                                                ultimo.y = fmaxf(ultimo.y, atual.y);
                                            } else { // Se tem espaço seguro entre eles, cria nova fita
                                                mesclados.push_back(atual);
                                            }
                                        }
                                        trechosPerigo = mesclados;
                                    }
                                }
                            }
                        }

                        rotaAtiva = (destinoTracado == indexUI);
                        Color corBotao;
                        if (rotaAtiva) corBotao = YELLOW; 
                        else if (hoverRota) corBotao = GREEN;
                        else corBotao = DARKGREEN;

                        DrawRectangleRec(btnRota, corBotao);
                        DrawRectangleLinesEx(btnRota, 3, BLACK); 
                        
                        int textW = MeasureText("TRACAR\n ROTA", 15);
                        if(rotaAtiva){
                            DrawText("TRACAR\n  ROTA", (int)(btnRota.x + (btnRota.width / 2) - (textW / 2)), (int)(btnRota.y + (alturaBtn/2)), 15, BLACK);
                        } else { 
                            DrawText("TRACAR\n  ROTA", (int)(btnRota.x + (btnRota.width / 2) - (textW / 2)), (int)(btnRota.y + (alturaBtn/2) - 5), 15, BLACK);
                        }
                    }
                    DrawText(TextFormat("Distância:\n %d AL", distanciaAnosLuz), boxX + 10, boxY + 85, 20, SKYBLUE);
                } else {
                    // --- DADOS DA ESTRELA ATUAL (UNIFICADO NA CAIXA ORIGINAL) ---
                    int linhaY = boxY + 90; // Começa a desenhar logo abaixo da barra de Ki

                    DrawText("LOCAL ATUAL (Nave Ancorada)", boxX + 10, linhaY, 10, YELLOW);
                    linhaY += 15;
                    
                    DrawText(estrelaUI->classificacao_cientifica.c_str(), boxX + 10, linhaY, 15, WHITE);
                    linhaY += 20;
                    
                    DrawText(TextFormat("Idade: %i Milhoes de Anos", estrelaUI->idade_milhoes_anos), boxX + 10, linhaY, 10, LIGHTGRAY);
                    linhaY += 20;
                    
                    DrawText("PLANETAS NA ORBITA:", boxX + 10, linhaY, 10, GRAY);
                    linhaY += 15;
                    
                    for (int p = 0; p < estrelaUI->planetas.size(); p++) {
                        // Define a cor: Cinza se for incompatível, Verde limão se tiver civilização
                        Color corPlaneta = (estrelaUI->planetas[p].tipo_vida == 1) ? DARKGRAY : LIME;
                        
                        // Desenha o Nome + (Tipo de Vida)
                        DrawText(TextFormat("- %s (%s)", estrelaUI->planetas[p].nome.c_str(), estrelaUI->planetas[p].desc_vida.c_str()), boxX + 20, linhaY, 10, corPlaneta);
                        linhaY += 15;
                    }
                }
            } else {
                // APENAS HOVER (A caixinha do mouse passando por cima)
                if (indexUI == estrelaAtualPlayer) {
                    DrawText("LOCAL ATUAL (Sua Nave)", boxX + 10, boxY + 120, 10, YELLOW);
                } else {
                    DrawText("[CLIQUE PARA SELECIONAR E TRACAR ROTA]", boxX + 10, boxY + 120, 10, LIGHTGRAY);
                    DrawText(TextFormat("Distância: %d AL", distanciaAnosLuz), boxX + 10, boxY + 85, 20, SKYBLUE);
                }
            }
        }

        //Barra de Ki do Jogador (Topo Centralizado)
        int barW = 400; int barH = 20;
        int barX = (screenWidth / 2) - (barW / 2);
        int barY = 20;

        // Fundo da barra
        DrawRectangle(barX, barY, barW, barH, Fade(BLACK, 0.8f));
        DrawRectangleLines(barX, barY, barW, barH, GRAY);

        // Calcula o preenchimento baseado no dímer (playerEscalaAtual)
        // Usamos a escala diretamente para o visual da barra ser suave
        float pctPreenchimento = (playerEscalaAtual - playerEscalaMinima) / (1.0f - playerEscalaMinima);
        if (pctPreenchimento < 0) pctPreenchimento = 0;

        // Cor da barra baseada na aura do jogador, mas com opacidade total
        Color corPreenchimento = { jogador->corAura.r, jogador->corAura.g, jogador->corAura.b, 255 };
        
        // Desenha o preenchimento
        DrawRectangle(barX + 2, barY + 2, (int)((barW - 4) * pctPreenchimento), barH - 4, corPreenchimento);

        // Texto com o valor exato (centralizado na barra)
        std::string textoPoder = TextFormat("PODER ATUAL: %d / %d", jogador->pdlAtual, jogador->pdlMaximo);
        int textoW = MeasureText(textoPoder.c_str(), 10);
        DrawText(textoPoder.c_str(), barX + (barW/2) - (textoW/2), barY + 5, 10, WHITE);

        EndDrawing();
    }
    
    CloseWindow();
    for (const auto& tex : bgTextures) UnloadTexture(tex);
     UnloadTexture(texFundoGalaxia);
    //UnloadTexture(texturaAura);    
    return 0;
}