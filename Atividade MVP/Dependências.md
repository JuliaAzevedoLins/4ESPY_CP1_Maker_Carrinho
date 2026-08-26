# Dependências do Projeto

As IDs abaixo correspondem às tarefas do [Backlog](Backlog.md).

| Tarefa | Depende de | Tipo de dependência | Consequência se atrasar |
|---|---|---|---|
| B02 — Aprovar orçamento | B01 | Técnica/financeira | Compra pode ser feita com componente incompatível |
| B03 — Comprar/separar itens | B02 | Financeira | Bloqueia medições, montagem e testes definitivos |
| B04 — Medir componentes | B03 | Material | Modelagem pode usar dimensões incorretas |
| B05 — Modelar chassi | B04 | Mecânica | Impressão e montagem física não começam |
| B06 — Revisar modelo | B05, B08 | Mecânica/elétrica | Risco de furos e espaços incompatíveis |
| B07 — Imprimir chassi | B06 | Fabricação | Bloqueia montagem final |
| B09 — Controle dos motores | B08, B03 | Técnica/material | Comunicação não poderá acionar o carrinho |
| B10 — Comunicação sem fio | B08, B03 | Técnica/material | Controle remoto não funciona |
| B11 — Parada de segurança | B08, B03 | Técnica/material | MVP fica sem requisito de segurança |
| B12 — Circuito definitivo | B03, B08 | Material/técnica | Integração não pode ser validada com segurança |
| B13 — Integração geral | B09, B10, B11, B12 | Técnica | MVP não pode ser testado |
| B14 — Teste do MVP | B13 | Validação | Não há comprovação do produto mínimo |
| B15 — Correções do MVP | B14 | Qualidade | Defeitos seguem para a montagem final |
| B16 — Montagem no chassi | B07, B12, B15 | Mecânica/técnica | Carenagem e teste final ficam bloqueados |
| B17 — Carenagem | B07, B16 | Mecânica/estética | Projeto não atinge a identidade visual proposta |
| B18 — Calibração final | B16, B17 | Qualidade | Movimento pode ficar instável |
| B19 — Teste final | B17, B18 | Validação | Não há evidência confiável para a entrega |
| B20 — Documentação final | B14, B19 | Documentação | GitHub não representa o protótipo entregue |
| B21 — Roteiro/contingência | B19, B20 | Apresentação | Demonstração fica vulnerável a falhas |
| B22 — Entrega | B20, B21 | Acadêmica | Entrega incompleta ou sem evidências |

## Caminho crítico

O caminho com menor tolerância a atraso é:

`B01 → B02 → B03 → B04 → B05 → B06 → B07 → B16 → B17 → B18 → B19 → B20 → B21 → B22`

Em paralelo, a frente eletrônica e de firmware segue:

`B03 + B08 → B09/B10/B11/B12 → B13 → B14 → B15 → B16`

## Dependências externas e mitigação

| Dependência externa | Risco | Mitigação |
|---|---|---|
| Entrega de componentes | Atraso ou item incompatível | Comprar até 28/08 e manter componentes de teste como contingência |
| Disponibilidade da impressora 3D | Fila, falha ou reimpressão | Reservar horário, imprimir teste de encaixe e concluir até 07/09 |
| Aplicativo/controlador Bluetooth | Incompatibilidade com celular | Testar em dois celulares e manter comandos documentados |
| Carga das pilhas | Queda de tensão durante a apresentação | Levar conjunto novo de pilhas e multímetro |
| Falha na demonstração ao vivo | Ambiente ou conexão | Gravar vídeo completo do teste final em 15/09 |
