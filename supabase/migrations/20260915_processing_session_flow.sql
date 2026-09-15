-- TAMPAÊ: encerramento controlado pela máquina
-- O enum processando já deve existir no banco de produção.

CREATE OR REPLACE FUNCTION public.registrar_coleta(p_machine_id uuid, p_device_token text, p_peso integer)
RETURNS TABLE(coleta_id uuid, pontos integer, sessao_id uuid)
LANGUAGE plpgsql SECURITY DEFINER SET search_path = public AS $$
DECLARE v_session_id uuid;
BEGIN
  IF NOT EXISTS (SELECT 1 FROM public.maquinas WHERE id=p_machine_id AND device_token=p_device_token AND ativa=true) THEN
    RAISE EXCEPTION 'Máquina não autorizada';
  END IF;
  SELECT s.id INTO v_session_id FROM public.sessoes s
  WHERE s.machine_id=p_machine_id AND s.evento_id=public.get_evento_atual()
    AND s.status IN ('aguardando'::public.session_status,'processando'::public.session_status)
    AND (s.status='processando'::public.session_status OR s.expira_em>now())
  ORDER BY s.criada_em DESC LIMIT 1 FOR UPDATE;
  IF v_session_id IS NULL THEN RAISE EXCEPTION 'Nenhuma sessão ativa'; END IF;
  RETURN QUERY INSERT INTO public.coletas(sessao_id,quantidade,pontos)
  VALUES(v_session_id,p_peso,p_peso) RETURNING id, public.coletas.pontos, sessao_id;
END; $$;

DROP FUNCTION IF EXISTS public.get_active_session(uuid,text);
CREATE FUNCTION public.get_active_session(p_machine_id uuid,p_device_token text)
RETURNS TABLE(session_id uuid,user_id uuid,nome text,evento_id uuid,status public.session_status)
LANGUAGE plpgsql SECURITY DEFINER SET search_path=public AS $$
BEGIN
  IF NOT EXISTS (SELECT 1 FROM public.maquinas WHERE id=p_machine_id AND device_token=p_device_token AND ativa=true) THEN RAISE EXCEPTION 'Máquina não autorizada'; END IF;
  RETURN QUERY SELECT s.id,s.user_id,COALESCE(p.nome,'Usuário'),s.evento_id,s.status
  FROM public.sessoes s LEFT JOIN public.profiles p ON p.id=s.user_id
  WHERE s.machine_id=p_machine_id AND s.evento_id=public.get_evento_atual()
    AND s.status IN ('aguardando'::public.session_status,'processando'::public.session_status)
    AND (s.status='processando'::public.session_status OR s.expira_em>now())
  ORDER BY s.criada_em DESC LIMIT 1;
END; $$;

CREATE OR REPLACE FUNCTION public.encerrar_sessao_usuario(p_session_id uuid)
RETURNS TABLE(pontos_sessao integer)
LANGUAGE plpgsql SECURITY DEFINER SET search_path=public AS $$
DECLARE v_status public.session_status;
BEGIN
  SELECT status INTO v_status FROM public.sessoes WHERE id=p_session_id AND user_id=auth.uid() FOR UPDATE;
  IF NOT FOUND THEN RAISE EXCEPTION 'Sessão não encontrada ou não pertence ao usuário'; END IF;
  IF v_status='concluida'::public.session_status THEN
    RETURN QUERY SELECT COALESCE((SELECT SUM(pontos)::integer FROM public.coletas WHERE sessao_id=p_session_id),0); RETURN;
  END IF;
  UPDATE public.sessoes SET status='processando'::public.session_status, expira_em=GREATEST(expira_em,now()+interval '2 minutes') WHERE id=p_session_id;
  RETURN QUERY SELECT 0;
END; $$;

CREATE OR REPLACE FUNCTION public.finalizar_sessao_maquina(p_machine_id uuid,p_device_token text,p_session_id uuid)
RETURNS TABLE(pontos_sessao integer)
LANGUAGE plpgsql SECURITY DEFINER SET search_path=public AS $$
DECLARE v_status public.session_status; v_pontos integer;
BEGIN
  IF NOT EXISTS (SELECT 1 FROM public.maquinas WHERE id=p_machine_id AND device_token=p_device_token AND ativa=true) THEN RAISE EXCEPTION 'Máquina não autorizada'; END IF;
  SELECT status INTO v_status FROM public.sessoes WHERE id=p_session_id AND machine_id=p_machine_id FOR UPDATE;
  IF NOT FOUND THEN RAISE EXCEPTION 'Sessão não encontrada'; END IF;
  SELECT COALESCE(SUM(pontos)::integer,0) INTO v_pontos FROM public.coletas WHERE sessao_id=p_session_id;
  IF v_status<>'concluida'::public.session_status THEN UPDATE public.sessoes SET status='concluida'::public.session_status,concluida_em=now() WHERE id=p_session_id; END IF;
  RETURN QUERY SELECT v_pontos;
END; $$;

GRANT EXECUTE ON FUNCTION public.registrar_coleta(uuid,text,integer) TO anon,authenticated;
GRANT EXECUTE ON FUNCTION public.get_active_session(uuid,text) TO anon,authenticated;
GRANT EXECUTE ON FUNCTION public.encerrar_sessao_usuario(uuid) TO authenticated;
GRANT EXECUTE ON FUNCTION public.finalizar_sessao_maquina(uuid,text,uuid) TO anon,authenticated;
