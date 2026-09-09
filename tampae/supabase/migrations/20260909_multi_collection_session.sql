-- TAMPAÊ: permite várias coletas na mesma sessão.
-- A sessão não é mais consumida pela primeira tampinha.
-- O encerramento continua sendo feito pelo celular, expiração ou máquina.

CREATE OR REPLACE FUNCTION public.registrar_coleta(
  p_machine_id uuid,
  p_device_token text,
  p_session_id uuid,
  p_tipo_coleta collection_type,
  p_quantidade_real integer DEFAULT NULL,
  p_quantidade_estimada integer DEFAULT NULL,
  p_peso_real_gramas numeric DEFAULT NULL,
  p_peso_estimado_gramas numeric DEFAULT NULL
)
RETURNS uuid
LANGUAGE plpgsql
SECURITY DEFINER
SET search_path TO 'public'
AS $$
DECLARE
  v_user_id uuid;
  v_evento_id uuid;
  v_evento_atual uuid;
  v_quantidade integer;
  v_pontos integer;
  v_collection_id uuid;
BEGIN
  IF NOT EXISTS (
    SELECT 1
    FROM public.machines m
    WHERE m.id = p_machine_id
      AND m.device_token = p_device_token
      AND m.status = 'ativa'
  ) THEN
    RAISE EXCEPTION 'token invalido para esta maquina';
  END IF;

  v_evento_atual := public.get_evento_atual();

  IF v_evento_atual IS NULL THEN
    RAISE EXCEPTION 'nenhum evento em_andamento';
  END IF;

  SELECT ms.user_id, ms.evento_id
  INTO v_user_id, v_evento_id
  FROM public.machine_sessions ms
  WHERE ms.id = p_session_id
    AND ms.machine_id = p_machine_id
    AND ms.status = 'aguardando'
    AND ms.expira_em > now()
  FOR UPDATE;

  IF v_user_id IS NULL THEN
    RAISE EXCEPTION 'sessao invalida ou expirada';
  END IF;

  IF v_evento_id IS DISTINCT FROM v_evento_atual THEN
    RAISE EXCEPTION 'sessao nao pertence ao evento atual';
  END IF;

  v_quantidade := COALESCE(
    p_quantidade_real,
    p_quantidade_estimada,
    0
  );

  -- Regra atual: 1 ponto por tampinha.
  v_pontos := v_quantidade;

  INSERT INTO public.collections (
    user_id,
    machine_id,
    evento_id,
    tipo_coleta,
    quantidade_real,
    quantidade_estimada,
    peso_real_gramas,
    peso_estimado_gramas,
    pontos
  )
  VALUES (
    v_user_id,
    p_machine_id,
    v_evento_id,
    p_tipo_coleta,
    p_quantidade_real,
    p_quantidade_estimada,
    p_peso_real_gramas,
    p_peso_estimado_gramas,
    v_pontos
  )
  RETURNING id INTO v_collection_id;

  -- Não encerra a sessão: várias tampinhas podem ser registradas.
  RETURN v_collection_id;
END;
$$;

GRANT EXECUTE ON FUNCTION public.registrar_coleta(uuid, text, uuid, collection_type, integer, integer, numeric, numeric) TO anon;
