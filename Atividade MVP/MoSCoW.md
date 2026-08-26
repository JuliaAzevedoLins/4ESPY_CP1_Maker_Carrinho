# Priorização MoSCoW

O método MoSCoW separa o que é obrigatório para a entrega do que agrega valor, mas pode ser retirado caso ameace o prazo de 16/09/2026.

## Must have — obrigatório

| ID | Funcionalidade | Justificativa |
|---|---|---|
| M01 | Movimentar-se para frente e para trás | Função básica do carrinho |
| M02 | Virar à esquerda e à direita | Necessário para controle direcional |
| M03 | Parar por comando do usuário | Requisito de controle e segurança |
| M04 | Receber comandos sem fio pelo ESP32 | Requisito central da proposta |
| M05 | Detectar obstáculos com HC-SR04 | Requisito funcional definido |
| M06 | Parar automaticamente antes da colisão | Torna a detecção útil e segura |
| M07 | Operar com alimentação por pilhas | Permite uso sem cabo conectado ao computador |
| M08 | Utilizar dois motores e driver TB6612FNG | Arquitetura definitiva definida no projeto |
| M09 | Possuir chassi impresso em 3D | Exigência física da atividade |
| M10 | Possuir carenagem inspirada no WALL-E | Identidade visual proposta |
| M11 | Manter componentes fixados e acessíveis | Evita falhas e permite manutenção |
| M12 | Disponibilizar código, documentação e evidências | Necessário para avaliação e rastreabilidade |

## Should have — importante

| ID | Funcionalidade | Condição |
|---|---|---|
| S01 | Controle proporcional de velocidade por PWM | Implementar após movimentos básicos estáveis |
| S02 | Ajuste simples da distância mínima no código | Facilita calibração nos testes |
| S03 | LED ou indicação visual de conexão/obstáculo | Melhora a demonstração e o diagnóstico |
| S04 | Carenagem removível sem ferramentas complexas | Facilita manutenção |
| S05 | Teste de autonomia de pelo menos 10 minutos | Confirma adequação da alimentação |

## Could have — desejável

| ID | Funcionalidade | Condição |
|---|---|---|
| C01 | Efeitos sonoros ou luzes decorativas | Somente se não afetar alimentação e prazo |
| C02 | Movimento dos braços ou cabeça | Somente após validação completa do carrinho |
| C03 | Diferentes modos de velocidade | Se o PWM estiver estável |
| C04 | Acabamento com pintura e adesivos detalhados | Se houver tempo após a montagem final |

## Won't have nesta entrega

| ID | Funcionalidade | Motivo |
|---|---|---|
| W01 | Navegação autônoma e desvio de rota | Amplia muito a complexidade do firmware |
| W02 | Aplicativo móvel próprio | Um aplicativo controlador existente é suficiente |
| W03 | Câmera e visão computacional | Fora do escopo e do orçamento |
| W04 | Comandos por voz | Não contribui para o núcleo do CP1 |
| W05 | Mapeamento do ambiente | Exigiria sensores e algoritmos adicionais |
| W06 | Esteiras mecânicas reais | A tração 2WD será apenas caracterizada visualmente |
