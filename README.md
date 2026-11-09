# diax-mkarmageddon

Recompilação estática para PC (Windows x64) do **Mortal Kombat: Armageddon** do Xbox original.

**Este repositório não contém o jogo.** Não há ISO, arquivos do disco, nem código gerado a partir do
executável do jogo. Você fornece a **sua própria ISO** do Mortal Kombat: Armageddon (Xbox original,
NTSC-U), e o código do jogo é gerado e compilado no seu PC.

## Instalação

1. Em **Releases**, baixe **`MKArmageddon-<versão>-starter.zip`** (não use os downloads "Source code" do
   GitHub: eles não trazem o launcher já compilado).
2. Extraia o ZIP inteiro numa pasta com permissão de escrita (por exemplo `C:\Jogos\MK Armageddon`).
3. Abra **`MK Armageddon.exe`**.
4. **Escolher a ISO...**: a sua ISO (ou o `default.xbe` de um disco já extraído). O launcher confere se é a
   versão suportada.
5. **Instalar e compilar**. O instalador:
   - instala o que faltar pelo `winget`, pedindo confirmação: Python 3 (+ `capstone`), CMake,
     Visual Studio Build Tools (C/C++) e Rust;
   - confere o disco e copia os arquivos para `mka\game`;
   - gera o código do jogo a partir do seu `default.xbe` (`tools_mka\generate.py`, vários minutos);
   - compila o jogo (a primeira compilação demora bastante) e abre o launcher de novo.
6. **Jogar**.

Requisitos: Windows 10/11 x64, placa de vídeo com Direct3D 11, ~10 GB livres para o disco e a compilação.

Também dá para clonar o repositório e rodar `Instalar MK Armageddon.cmd`, que faz o mesmo e pede a ISO.
`scripts\build-starter.ps1` monta o ZIP da release.

## O launcher

Uma janela de configurações simples:

- **Disco**: escolher a ISO; instalar e compilar.
- **Vídeo**: resolução interna (480p a 4K), proporção 4:3 / 16:9 / 21:9 (widescreen), janela ou tela cheia,
  tamanho da janela, anti-aliasing (FXAA/SSAA), V-Sync.
- **Jogo**: texturas HD (e os botões para baixá-las), pular as aberturas, sessão com tudo desbloqueado (numa
  cópia dos saves), 1 ou 2 jogadores, jogador 1 só no teclado, botões do controle (Xbox, DualShock 4,
  DualSense) e **Adicionar à Steam** (atalho de jogo não-Steam; feche a Steam antes).

Com dois controles, o segundo jogador entra apertando Start, como no console. O mouse funciona nos menus (no
menu principal e na seleção de lutadores o item embaixo do ponteiro é selecionado).

Konquest, Krypt e Kreate a Fighter pedem um perfil: no menu principal aperte **L / L2 (ENTER PROFILE KODE)**
e digite o kode do seu perfil (ou crie um em PROFILES). Os saves ficam em `mka\save`.

## Texturas HD

O jogo usa os dois pacotes de texturas HD feitos pela comunidade para a versão de PS2 no PCSX2. Eles não vêm
junto: baixe-os das páginas dos autores (veja os créditos). Os dois juntos dão o melhor resultado.

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

## Estrutura

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
- **[dsp56300](https://github.com/mborgerson/dsp56300)**, de **Matt Borgerson**: a emulação do DSP de áudio do Xbox.
- **[Capstone](https://www.capstone-engine.org/)**: o desmontador usado na recompilação.
- **Texturas HD**:
  - **[Mortal Kombat Armageddon PCSX2 Remaster/Remake: Arts and Menus](https://github.com/UstanakChaos/Mortal-Kombat-Armageddon-PCSX2-Remaster-Remake-Arts-and-Menus)**
    (artes, menus e personagens), de **UstanakChaos**, com a ajuda de **@Mikachuuu_** em muitos personagens;
  - **[Mortal Kombat Armageddon HD Texture Pack](https://github.com/ckiscxrsed/Mortal-Kombat-Armageddon-HD-Textures)**
    (upscale da versão de PS2), de **ckiscxrsed**.

  Todo o crédito das texturas é dos seus autores. Elas não vêm com este projeto: baixe-as das páginas deles.

Este projeto **não tem nenhum fim lucrativo**: não é vendido, não aceita doações e não é distribuído com
fins comerciais.

Mortal Kombat é marca registrada da Warner Bros. Entertainment Inc. Mortal Kombat: Armageddon foi
desenvolvido pela Midway. Este é um projeto de fã, sem afiliação com a Warner Bros. ou a Midway, e não
contém nenhum conteúdo do jogo. Use somente com uma cópia original que você possui.
