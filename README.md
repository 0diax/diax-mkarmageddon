# diax-mkarmageddon

<p align="center">
  <a href="https://github.com/0diax/diax-mkarmageddon/releases/latest"><img src="https://img.shields.io/badge/Download-Latest-2563eb?style=for-the-badge&amp;logo=github&amp;logoColor=white" alt="Baixar a versão mais recente"></a>
  <a href="https://github.com/0diax/diax-mkarmageddon/releases/latest"><img src="https://img.shields.io/github/v/release/0diax/diax-mkarmageddon?style=for-the-badge&amp;logo=github&amp;label=Latest%20release&amp;cacheSeconds=300" alt="Versão mais recente"></a>
  <a href="https://github.com/0diax/diax-mkarmageddon/releases"><img src="https://img.shields.io/github/downloads/0diax/diax-mkarmageddon/total?style=for-the-badge&amp;logo=github&amp;label=Release%20downloads" alt="Downloads das releases"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-MIT-blue?style=for-the-badge" alt="Licença MIT"></a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Progresso-82%25-22c55e?style=for-the-badge" alt="Progresso: 82%">
</p>

O **diax-mkarmageddon** é um port nativo para PC do **Mortal Kombat: Armageddon**, feito pela recompilação
estática do jogo original do Xbox.

> [!IMPORTANT]
> Este projeto não inclui o jogo. Você precisa da **sua própria ISO** do Mortal Kombat: Armageddon do
> **Xbox original, versão NTSC-U (EUA)**. O código do jogo é gerado e compilado no seu PC a partir dela.

## Sumário

- [Progresso](#progresso)
- [Requisitos](#requisitos)
- [Instalação](#instalação)
- [Atualização](#atualização)
- [Recursos](#recursos)
- [Texturas HD](#texturas-hd)
- [Problemas conhecidos](#problemas-conhecidos)
- [FAQ](#faq)
- [Compilação](#compilação)
- [Créditos](#créditos)

## Progresso

```
█████████████████████████████████████████░░░░░░░░░  82%
```

A barra vem de uma análise do código gerado e de testes de cada modo no jogo: dos 19 itens abaixo, os
funcionando contam inteiros e os parciais contam metade ((14 + 3 × ½) / 19 = 82%).

**Código traduzido:** 30.157 funções do executável do jogo. Ficam 10 pontos de chamada sem destino
identificado (a maioria com endereços inválidos, falsos positivos da análise), e nenhum deles foi atingido
nos testes.

| Item | Situação | Observação |
|---|---|---|
| Tradução do código do jogo | ✅ Funcionando | 30.157 funções; 10 pontos sem destino, nunca atingidos |
| Instalação a partir da ISO | ✅ Funcionando | O launcher confere o disco, gera o código e compila |
| Gráficos (Direct3D 11) | ✅ Funcionando | Resolução interna de 480p a 4K, FXAA e SSAA |
| Vídeos (abertura, cenas) | ✅ Funcionando | |
| Controles | ✅ Funcionando | Xbox, DualShock 4, DualSense, teclado; mouse nos menus |
| 2 jogadores locais | ✅ Funcionando | O segundo controle entra apertando Start |
| Perfis e saves | ✅ Funcionando | Criar perfil, entrar com o kode, salvar |
| Kombat (Arcade, Versus, Practice) | ✅ Funcionando | Seleção de lutadores e lutas |
| Konquest | 🟡 Parcial | Abertura, cenas e começo testados; a história completa ainda não foi jogada até o fim |
| Motor Kombat | ✅ Funcionando | Pistas, veículos e corridas |
| Kreate a Fighter | ✅ Funcionando | Editor de lutador (corpo, roupas, estilos) |
| The Krypt | ✅ Funcionando | Grade de itens e compras com koins |
| Extras | ✅ Funcionando | |
| Options | ✅ Funcionando | Jogo, áudio, vídeo e controles |
| Widescreen 16:9 e 21:9 | 🟡 Parcial | A câmera abre em widescreen; menus e HUD em 2D ficam esticados |
| Texturas HD | ✅ Funcionando | Os dois pacotes da comunidade (veja abaixo) |
| Áudio | 🟡 Parcial | Música, vozes e efeitos funcionam; há relatos de estalos e, raramente, o som não inicia |
| Multiplayer online | ❌ Não faz parte desta versão | |
| Xbox Live | ❌ Sem suporte | O serviço original do Xbox não existe mais |

## Requisitos

- Windows 10 ou 11, 64 bits
- Placa de vídeo com Direct3D 11
- Cerca de 10 GB livres (o disco copiado e a compilação)
- Internet na primeira instalação (para baixar as ferramentas de compilação)
- Controle, ou teclado e mouse

## Instalação

1. Baixe a [versão mais recente](https://github.com/0diax/diax-mkarmageddon/releases/latest):
   **`MKArmageddon-<versão>-starter.zip`** (não use os downloads "Source code" do GitHub).
2. Extraia o ZIP inteiro numa pasta com permissão de escrita, por exemplo `C:\Jogos\MK Armageddon`.
3. Abra **`MK Armageddon.exe`**.
4. Clique em **Escolher a ISO...** e selecione a sua ISO (ou o `default.xbe` de um disco já extraído).
5. Clique em **Instalar e compilar**. Na primeira vez ele instala o que faltar pelo `winget`, perguntando
   antes (Python 3 com `capstone`, CMake, Visual Studio Build Tools e Rust), copia o disco, gera o código do
   jogo e compila. Leva vários minutos; no fim o launcher abre de novo.
6. Clique em **Jogar**.

## Atualização

Baixe o ZIP da nova versão e extraia numa pasta nova. Para levar o seu progresso, copie a pasta `mka\save`
da instalação antiga para a nova, e escolha a ISO e instale de novo.

## Recursos

- **Launcher** com a escolha da ISO, a instalação e as configurações de vídeo, jogo e controles.
- **Resolução interna** de 480p (a do Xbox) a 4K, **anti-aliasing** FXAA ou SSAA e **V-Sync**.
- **Widescreen** 16:9 e 21:9, além do 4:3 original.
- **Tela cheia** sem bordas ou janela; F11 alterna durante o jogo.
- **Configurações de vídeo dentro do jogo**, em Options > Video (RB no controle ou V no teclado).
- **Controles** de Xbox e PlayStation (DualShock 4 e DualSense), com troca de botões, e teclado.
- **Mouse nos menus**: no menu principal e na seleção de lutadores, o item embaixo do ponteiro é selecionado.
- **2 jogadores locais**; com um controle só, o teclado pode ser o jogador 1.
- **Texturas HD** com os dois pacotes da comunidade.
- **Pular os vídeos de abertura**.
- **Sessão com tudo desbloqueado**, numa cópia dos saves: nada dela vai para o seu perfil.
- **Adicionar à Steam** como jogo não-Steam (feche a Steam antes).

## Texturas HD

O jogo usa os dois pacotes de texturas HD feitos pela comunidade para a versão de PS2 no PCSX2. Eles não vêm
junto: baixe-os das páginas dos autores. Os dois juntos dão o melhor resultado.

1. No launcher, **Baixar pacote HD (PS2)** abre a página de
   [Mortal-Kombat-Armageddon-HD-Textures](https://github.com/ckiscxrsed/Mortal-Kombat-Armageddon-HD-Textures/releases)
   (`SLUS-21410.rar`; as versões mais novas estão no Google Drive indicado lá), e **Baixar remake HD** abre a de
   [Mortal-Kombat-Armageddon-PCSX2-Remaster-Remake-Arts-and-Menus](https://github.com/UstanakChaos/Mortal-Kombat-Armageddon-PCSX2-Remaster-Remake-Arts-and-Menus/releases)
   (`MK.Armageddon.Remake.zip`).
2. **Abrir a pasta** cria e abre `hd-textures`, ao lado de `mka`.
3. Extraia os downloads ali, do jeito que vieram: o layout do PCSX2 (`SLUS-21410\replacements\...`) e o do
   remake (`MK Armageddon Remake\SLUS-21543\...`) servem como estão. O remake é usado primeiro e o pacote de
   PS2 completa o resto.
4. Reabra o launcher e ligue **Texturas HD**.

O jogo reconhece cada textura pela aparência (as do PS2 têm outros nomes) e guarda o que aprendeu num arquivo
de índice na pasta do pacote; o primeiro início com um pacote novo demora mais. As pastas opcionais dos
pacotes (`Optional Replacements`, `Bonus Test`) e as que começam com `_` ficam de fora.

## Problemas conhecidos

- **Áudio:** há relatos de estalos no som; nos testes a saída de áudio não teve cortes. Raramente o som não
  inicia; feche e abra o jogo de novo.
- **Widescreen:** a câmera abre corretamente, mas os menus e o HUD em 2D ficam esticados.
- **Konquest:** o começo funciona; a história completa ainda não foi testada até o fim.
- Algum problema gráfico pode aparecer: [abra uma issue](https://github.com/0diax/diax-mkarmageddon/issues)
  com uma captura de tela e o arquivo `mka\stderr.txt`.

## FAQ

### O launcher diz que o meu disco não é a versão suportada

Use a ISO original do **Xbox original, NTSC-U (EUA)**. Versões de outras regiões, a do PS2 e imagens
convertidas não são aceitas: o instalador confere o `default.xbe` do disco.

### Onde ficam os saves, as configurações e os registros?

| O quê | Onde |
|---|---|
| Saves e perfis | `mka\save` |
| Configurações de vídeo | `mka\video.ini` |
| Configurações do launcher | `mka\launcher.ini` |
| Registro do jogo | `mka\stderr.txt` |
| Registro da instalação | `install.log`, na pasta do ZIP |

### Konquest, Krypt e Kreate a Fighter pedem um perfil

No menu principal, aperte **L / L2 (ENTER PROFILE KODE)** e digite o kode do seu perfil, ou crie um em
**PROFILES**.

### Quais são os controles do teclado?

| Ação | Tecla | Botão do controle |
|---|---|---|
| Andar | W A S D ou setas | Direcional |
| Soco | J | X / Quadrado |
| Gancho | U | Y / Triângulo |
| Chute | K | A / Cruz |
| Arremesso | I | B / Círculo |
| Defesa | L ou Espaço | RT / R2 |
| Estilo de luta | O | LT / L2 |
| Arma | N | RB / R1 |
| Agarrão | H | LB / L1 |
| Pausa | Esc | Start / Options |
| Voltar | Tab | Back / Share |
| Tela cheia | F11 | |
| Acelerar (Motor Kombat) | Espaço | |

Os botões do controle podem ser trocados no launcher, em **Configurar botões do controle**.

### Como jogar em dois?

Ligue dois controles: o segundo jogador entra apertando Start, como no console. Com um controle só, escolha
**Jogador 1 usa: Só teclado** no launcher para o teclado ser o jogador 1 e o controle o jogador 2.

### Como melhorar o desempenho?

Diminua a **Resolução interna** e use **FXAA** em vez de **SSAA** no launcher.

## Compilação

Também dá para clonar o repositório e rodar **`Instalar MK Armageddon.cmd`**, que pede a ISO e faz o mesmo
que o launcher. `scripts\build-starter.ps1` monta o ZIP da release.

| Pasta | Conteúdo |
|---|---|
| `mka/src` | código escrito à mão: `main.c`, substituições de funções do jogo (`recomp_manual.c`), corrotinas (`mka_process.c`) |
| `mka/launcher` | o launcher (C, Win32) |
| `tools_mka` | `disc.py` (lê a ISO), `generate.py` (gera o código), sementes de funções e ferramentas de análise |
| `scripts` | `install.ps1` (instalação e compilação), `build-starter.ps1` (o ZIP da release) |
| `xboxrecomp` | runtime e recompilador estático de Xbox, com as mudanças deste projeto (licença MIT) |
| `third_party/dsp56300` | emulador do DSP de áudio do Xbox (licença MIT) |

Gerado no seu PC e nunca versionado: `mka/game` (o disco), `mka/src/recomp/gen` (o código do jogo),
`mka/build`, `hd-textures` (baixadas das páginas dos autores) e os saves (`mka/save*`).

## Créditos

Este projeto só existe graças ao trabalho destas pessoas e projetos:

- **[xboxrecomp](https://github.com/portingpete/xboxrecomp)**, de **sp00nz** e **portingpete**: o runtime e o
  recompilador estático de Xbox sobre os quais tudo aqui roda (licença MIT).
- **[Black 2006 Recomp](https://github.com/portingpete/Black-2006-Recomp)**, de **portingpete**: o modelo de
  instalação (o jogador traz o próprio disco e o jogo é gerado no PC dele) e o runtime de que este projeto partiu.
- **[coop-anything](https://github.com/cranebeaker/coop-anything)**, de **cranebeaker**: a referência estudada
  para um modo online, que não faz parte desta versão.
- **[OpenShao](https://github.com/OAleex/OpenShao)**, de **OAleex**: a inspiração para esta página.
- **[dsp56300](https://github.com/mborgerson/dsp56300)**, de **Matt Borgerson**: a emulação do DSP de áudio do Xbox.
- **[Capstone](https://www.capstone-engine.org/)**: o desmontador usado na recompilação.
- **Texturas HD**:
  - **[Mortal Kombat Armageddon PCSX2 Remaster/Remake: Arts and Menus](https://github.com/UstanakChaos/Mortal-Kombat-Armageddon-PCSX2-Remaster-Remake-Arts-and-Menus)**
    (artes, menus e personagens), de **UstanakChaos**, com a ajuda de **@Mikachuuu_** em muitos personagens;
  - **[Mortal Kombat Armageddon HD Texture Pack](https://github.com/ckiscxrsed/Mortal-Kombat-Armageddon-HD-Textures)**
    (upscale da versão de PS2), de **ckiscxrsed**.

  Todo o crédito das texturas é dos seus autores. Elas não vêm com este projeto: baixe-as das páginas deles.

Este projeto **não tem nenhum fim lucrativo**: não é vendido, não aceita doações e não é distribuído com
fins comerciais. O código próprio deste projeto está sob a [licença MIT](LICENSE).

Mortal Kombat é marca registrada da Warner Bros. Entertainment Inc. Mortal Kombat: Armageddon foi
desenvolvido pela Midway. Este é um projeto de fã, sem afiliação com a Warner Bros. ou a Midway, e não
contém nenhum conteúdo do jogo. Use somente com uma cópia original que você possui.
