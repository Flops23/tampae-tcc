// CORREÇÃO: ao registrar coleta, não enviar 0.00 como peso quando a coleta foi feita apenas por unidades/LDR.
// Para aplicar no arquivo existente, substitua a montagem do JSON de registrar_coleta pela lógica abaixo.
//
// Regra:
// - LDR/unidades: p_quantidade_real preenchido; p_peso_real_gramas = null
// - balança: p_quantidade_real preenchido; p_peso_real_gramas = peso medido
// - combinação: quantidade total preenchida; peso somente se houve pesagem
//
// ATENÇÃO: este arquivo no GitHub está atualmente sem conteúdo legível pelo conector.
// A alteração abaixo preserva a regra e serve como marcador de correção até o arquivo-fonte
// completo ser sincronizado no repositório.
