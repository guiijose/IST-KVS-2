#ifndef COMMON_PROTOCOL_H
#define COMMON_PROTOCOL_H

// Opcodes for client-server communication
// estes opcodes sao usados num switch case para determinar o que fazer com a mensagem recebida no server
// usam estes opcodes tambem nos clientes quando enviam mensagens para o server
enum {
  OP_CODE_CONNECT = 1,
  // TODO mais opcodes para cada operacao
  OP_CODE_DISCONNECT = 2,
  OP_CODE_SUCCESSFUL_CONNNECTION = 3,
};

#endif  // COMMON_PROTOCOL_H
