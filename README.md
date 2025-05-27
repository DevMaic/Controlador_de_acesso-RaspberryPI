<h1>
  <p align="center" width="100%">
    <img width="30%" src="https://softex.br/wp-content/uploads/2024/09/EmbarcaTech_logo_Azul-1030x428.png">
  </p>
</h1>

# ✨Tecnologias
Esse projeto foi desenvolvido com as seguintes tecnologias.
- Placa Raspberry Pi Pico W
- Raspberry Pi Pico SDK
- C/C++

# 💻Projeto
Projeto Desenvolvido durante a residência em microcontrolados e sistemas embarcados para estudantes de nível superior ofertado pela CEPEDI e SOFTEX, polo Juazeiro-BA, na Universidade Federal do Vale do São Francisco (UNIVASF), que tem como objetivo ser um sistema de controle de acesso utilizando a placa BitDogLab com Raspberry PI-Pico.

# 🚀Como rodar
### **Softwares Necessários**
1. **VS Code** com a extensão **Raspberry Pi Pico** instalada.
2. **CMake** e **Ninja** configurados.
3. **SDK do Raspberry Pi Pico** corretamente configurado.

### **Clonando o Repositório**
Para começar, clone o repositório no seu computador:
```bash
git clone https://github.com/DevMaic/Controlador_de_acesso-RaspberryPI
cd Controlador_de_acesso-RaspberryPI
```
---


### **Execução na Placa BitDogLab**
#### **1. Upload de Arquivo `PiscaLed.uf2`**
1. Importe o projeto utilizando a extensão do VSCode, e o compile.
2. Abra a pasta `build` que será gerada na compilação.
3. Aperte o botão **BOOTSEL** no microcontrolador Raspberry Pi Pico W.
4. Ao mesmo tempo, aperte o botão de **Reset**..
5. Mova o arquivo `PiscaLed.uf2` para a placa de desenvolvimento.
#### **2. Acompanhar Execução do Programa**
1. Assim que o código estiver na placa, observe a contagem de usuários e vagas disponíveis no display.
2. Pressione os botões A e B da placa, para adicionar e remover usuários.
3. Observe também o código de cores pra o número de vagas nos LEDs RGB.
   
