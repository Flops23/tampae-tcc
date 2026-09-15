CREATE OR REPLACE FUNCTION public.encerrar_sessao_usuario(p_session_id uuid)
RETURNS TABLE(pontos_sessao integer)
LANGUAGE plpgsql
SECURITY DEFINER
SET search_path TO 'public'
AS $function$
DECLARE
  v_user_id uuid;
  v_machine_id uuid;
  v_evento_id uuid;
  v_criado_em timestamptz;
  v_pontos integer := 0;
BEGIN
  SELECT
    ms.user_id,
    ms.machine_id,
    ms.evento_id,
    ms.criado_em
  INTO
    v_user_id,
    v_machine_id,
    v_evento_id,
    v_criado_em
  FROM public.machine_sessions ms
  WHERE ms.id = p_session_id;

  IF v_user_id IS NULL THEN
    RAISE EXCEPTION 'sessao nao encontrada';
  END IF;

  IF v_user_id IS DISTINCT FROM auth.uid() THEN
    RAISE EXCEPTION 'sessao nao pertence ao usuario autenticado';
  END IF;

  IF EXISTS (
    SELECT 1
    FROM public.machine_sessions ms
    WHERE ms.id = p_session_id
      AND ms.status = 'concluida'
  ) THEN
    SELECT COALESCE(SUM(c.pontos), 0)::integer
    INTO v_pontos
    FROM public.collections c
    WHERE c.user_id = v_user_id
      AND c.machine_id = v_machine_id
      AND c.evento_id = v_evento_id
      AND c.criado_em >= v_criado_em
      AND c.criado_em <= COALESCE(
        (
          SELECT ms2.concluida_em
          FROM public.machine_sessions ms2
          WHERE ms2.id = p_session_id
        ),
        now()
      );

    RETURN QUERY SELECT COALESCE(v_pontos, 0);
    RETURN;
  END IF;

  IF NOT EXISTS (
    SELECT 1
    FROM public.machine_sessions ms
    WHERE ms.id = p_session_id
      AND ms.status = 'aguardando'
  ) THEN
    RETURN QUERY SELECT 0;
    RETURN;
  END IF;

  UPDATE public.machine_sessions
  SET expira_em = GREATEST(expira_em, now() + interval '5 seconds')
  WHERE id = p_session_id
    AND status = 'aguardando';

  PERFORM pg_sleep(5);

  SELECT
    ms.user_id,
    ms.machine_id,
    ms.evento_id,
    ms.criado_em
  INTO
    v_user_id,
    v_machine_id,
    v_evento_id,
    v_criado_em
  FROM public.machine_sessions ms
  WHERE ms.id = p_session_id
  FOR UPDATE;

  IF v_user_id IS DISTINCT FROM auth.uid() THEN
    RAISE EXCEPTION 'sessao nao pertence ao usuario autenticado';
  END IF;

  SELECT COALESCE(SUM(c.pontos), 0)::integer
  INTO v_pontos
  FROM public.collections c
  WHERE c.user_id = v_user_id
    AND c.machine_id = v_machine_id
    AND c.evento_id = v_evento_id
    AND c.criado_em >= v_criado_em
    AND c.criado_em <= now();

  UPDATE public.machine_sessions
  SET
    status = 'concluida',
    concluida_em = now()
  WHERE id = p_session_id
    AND status = 'aguardando';

  RETURN QUERY SELECT COALESCE(v_pontos, 0);
END;
$function$;
