# MVP — Carrinho Robótico WALL-E

## Objetivo

Entregar um carrinho robótico controlado sem fio, capaz de se deslocar em quatro direções e de parar com segurança ao detectar um obstáculo, evoluindo depois para a versão física e visual completa inspirada no WALL-E.

## Quando estará minimamente funcional?

O **MVP deverá estar pronto até 09/09/2026**. Nessa data, o carrinho poderá utilizar uma base provisória, mas deverá reunir em um único protótipo:

- ESP32 programado e operante;
- driver TB6612FNG acionando dois motores DC;
- comandos sem fio para frente, ré, esquerda, direita e parada;
- sensor HC-SR04 integrado à lógica de segurança;
- parada automática quando houver obstáculo abaixo da distância configurada;
- alimentação por pilhas, sem depender do computador;
- teste funcional documentado em vídeo.

### Critérios de aceite do MVP

O MVP será aceito quando, em **três execuções consecutivas**:

1. conectar ao controle sem fio;
2. executar todos os comandos de movimento;
3. parar diante de um obstáculo sem colisão;
4. voltar a aceitar comandos após a retirada do obstáculo;
5. funcionar alimentado apenas pelas pilhas.

## Quando estará concluído?

O **projeto completo deverá estar pronto em 16/09/2026**, data da entrega do CP1. Além de cumprir todos os critérios do MVP, deverá possuir:

- chassi final modelado e impresso em 3D;
- componentes firmemente fixados e fiação organizada;
- carenagem reconhecível como WALL-E, removível para manutenção;
- centro de gravidade e rodas ajustados para movimentação estável;
- testes finais de movimento, sensor, conexão e autonomia curta;
- código-fonte final e documentação atualizados no GitHub;
- fotos e vídeo de demonstração do protótipo concluído.

## Cronograma de marcos

| Marco | Data limite | Resultado esperado |
|---|---:|---|
| Definições e compras fechadas | 28/08/2026 | Componentes confirmados e pedidos realizados |
| Modelo 3D e firmware base | 02/09/2026 | Chassi pronto para impressão e código migrado para ESP32 |
| Integração eletrônica | 06/09/2026 | Motores, controle sem fio e sensor funcionando em conjunto |
| MVP validado | 09/09/2026 | Protótipo móvel, seguro e independente do computador |
| Montagem física final | 13/09/2026 | Chassi, componentes e carenagem montados |
| Testes e documentação final | 15/09/2026 | Evidências e arquivos revisados |
| Entrega do CP1 | 16/09/2026 | Projeto completo apresentado |

## Fora do escopo desta entrega

- navegação totalmente autônoma;
- câmera, reconhecimento de imagem ou voz;
- aplicativo próprio;
- mapeamento de ambientes;
- esteiras motorizadas reais;
- bateria recarregável e estação de recarga.
