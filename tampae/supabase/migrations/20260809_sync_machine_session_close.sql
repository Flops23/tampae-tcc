-- TampAê: sincronização de encerramento entre celular e máquina.
-- Execute este arquivo no SQL Editor do Supabase antes de testar o novo qr.js.

CREATE OR REPLACE FUNCTION public.encerrar_sessao_usuario(
  p_session_id uuid
)
RETURNS TABLE (
  session_id uuid,
  pontos_sessao integer
)
LANGUAGE plpgsql
SECURITY DEFINER
SET search_path = public
AS $$
DECLARE
  v_user_id uuid;
  v_machine_id uuid;
  v_evento_id uuid;
  v_criado_em timestamptz;
  v_pontos integer;
BEGIN
  SELECT ms.user_id, ms.machine_id, ms.evento_id, ms.criado_em
  INTO v_user_id, v_machine_id, v_evento_id, v_criado_em
  FROM public.machine_sessions ms
  WHERE ms.id = p_session_id
    AND ms.status = 'aguardando'
  FOR UPDATE;

  IF v_user_id IS NULL THEN
    RETURN QUERY SELECT p_session_id, 0;
    RETURN;
  END IF;

  IF auth.uid() IS NULL OR auth.uid() <> v_user_id THEN
    RAISE EXCEPTION 'sessao nao pertence ao usuario autenticado';
  END IF;

  SELECT COALESCE(SUM(c.pontos), 0)::integer
  INTO v_pontos
  FROM public.collections c
  WHERE c.user_id = v_user_id
    AND c.machine_id = v_machine_id
    AND c.evento_id IS NOT DISTINCT FROM v_evento_id
    AND c.criado_em >= v_criado_em;

  UPDATE public.machine_sessions
  SET status = 'concluida', concluida_em = now()
  WHERE id = p_session_id;

  RETURN QUERY SELECT p_session_id, v_pontos;
END;
$$;

CREATE OR REPLACE FUNCTION public.encerrar_sessao_maquina(
  p_machine_id uuid,
  p_device_token text,
  p_session_id uuid
)
RETURNS TABLE (
  session_id uuid,
  pontos_sessao integer
)
LANGUAGE plpgsql
SECURITY DEFINER
SET search_path = public
AS $$
DECLARE
  v_user_id uuid;
  v_evento_id uuid;
  v_criado_em timestamptz;
  v_pontos integer;
BEGIN
  IF NOT EXISTS (
    SELECT 1 FROM public.machines m
    WHERE m.id = p_machine_id
      AND m.device_token = p_device_token
  ) THEN
    RAISE EXCEPTION 'token invalido para esta maquina';
  END IF;

  SELECT ms.user_id, ms.evento_id, ms.criado_em
  INTO v_user_id, v_evento_id, v_criado_em
  FROM public.machine_sessions ms
  WHERE ms.id = p_session_id
    AND ms.machine_id = p_machine_id
    AND ms.status = 'aguardando'
  FOR UPDATE;

  IF v_user_id IS NULL THEN
    RETURN QUERY SELECT p_session_id, 0;
    RETURN;
  END IF;

  SELECT COALESCE(SUM(c.pontos), 0)::integer
  INTO v_pontos
  FROM public.collections c
  WHERE c.user_id = v_user_id
    AND c.machine_id = p_machine_id
    AND c.evento_id IS NOT DISTINCT FROM v_evento_id
    AND c.criado_em >= v_criado_em;

  UPDATE public.machine_sessions
  SET status = 'concluida', concluida_em = now()
  WHERE id = p_session_id;

  RETURN QUERY SELECT p_session_id, v_pontos;
END;
$$;

REVOKE EXECUTE ON FUNCTION public.encerrar_sessao_usuario(uuid) FROM PUBLIC;
REVOKE EXECUTE ON FUNCTION public.encerrar_sessao_maquina(uuid, text, uuid) FROM PUBLIC;

GRANT EXECUTE ON FUNCTION public.encerrar_sessao_usuario(uuid) TO authenticated;
GRANT EXECUTE ON FUNCTION public.encerrar_sessao_maquina(uuid, text, uuid) TO anon;
