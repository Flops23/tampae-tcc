-- ============================================================
-- TAMPAE - TESTE DE PONTUACAO / SESSAO CONTINUA
-- ============================================================
-- IMPORTANTE:
-- A RPC antiga registrar_coleta fecha a machine_session depois
-- de cada coleta. Por isso ela NAO deve ser usada neste teste.
-- Estas RPCs novas permitem varias coletas na mesma sessao.
--
-- Execute este arquivo no SQL Editor do Supabase antes de usar
-- hardware/teste_pontuacao.ino.
-- ============================================================

create or replace function public.get_active_session_continua(
  p_machine_id uuid,
  p_device_token text
)
returns table (
  session_id uuid,
  user_id uuid,
  nome text,
  evento_id uuid
)
language plpgsql
security definer
set search_path = public
as $$
begin
  if not exists (
    select 1
    from public.machines m
    where m.id = p_machine_id
      and m.device_token = p_device_token
  ) then
    raise exception 'token invalido para esta maquina';
  end if;

  return query
  select
    ms.id,
    ms.user_id,
    p.nome,
    ms.evento_id
  from public.machine_sessions ms
  join public.profiles p on p.id = ms.user_id
  where ms.machine_id = p_machine_id
    and ms.status = 'aguardando'
  order by ms.criado_em desc
  limit 1;
end;
$$;


create or replace function public.registrar_coleta_continua(
  p_machine_id uuid,
  p_device_token text,
  p_session_id uuid,
  p_tipo_coleta collection_type,
  p_quantidade_real integer default null,
  p_quantidade_estimada integer default null,
  p_peso_real_gramas numeric default null,
  p_peso_estimado_gramas numeric default null
)
returns uuid
language plpgsql
security definer
set search_path = public
as $$
declare
  v_user_id uuid;
  v_evento_id uuid;
  v_quantidade integer;
  v_pontos integer;
  v_collection_id uuid;
begin
  if not exists (
    select 1
    from public.machines m
    where m.id = p_machine_id
      and m.device_token = p_device_token
  ) then
    raise exception 'token invalido para esta maquina';
  end if;

  select ms.user_id, ms.evento_id
    into v_user_id, v_evento_id
  from public.machine_sessions ms
  where ms.id = p_session_id
    and ms.machine_id = p_machine_id
    and ms.status = 'aguardando'
  for update;

  if v_user_id is null then
    raise exception 'sessao invalida ou encerrada';
  end if;

  v_quantidade := coalesce(p_quantidade_real, p_quantidade_estimada, 0);

  if v_quantidade <= 0 then
    raise exception 'quantidade de tampinhas deve ser maior que zero';
  end if;

  -- Regra atual: 1 ponto por tampinha.
  v_pontos := v_quantidade;

  insert into public.collections (
    user_id,
    machine_id,
    evento_id,
    tipo_coleta,
    quantidade_real,
    quantidade_estimada,
    peso_real_gramas,
    peso_estimado_gramas,
    pontos
  ) values (
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
  returning id into v_collection_id;

  -- DELIBERADAMENTE NAO fecha machine_sessions.
  -- A sessao continua ativa para a proxima passagem.

  return v_collection_id;
end;
$$;


create or replace function public.encerrar_sessao_maquina(
  p_machine_id uuid,
  p_device_token text,
  p_session_id uuid
)
returns boolean
language plpgsql
security definer
set search_path = public
as $$
declare
  v_atualizadas integer;
begin
  if not exists (
    select 1
    from public.machines m
    where m.id = p_machine_id
      and m.device_token = p_device_token
  ) then
    raise exception 'token invalido para esta maquina';
  end if;

  update public.machine_sessions
  set
    status = 'concluida',
    concluida_em = now()
  where id = p_session_id
    and machine_id = p_machine_id
    and status = 'aguardando';

  get diagnostics v_atualizadas = row_count;
  return v_atualizadas > 0;
end;
$$;

revoke execute on function public.get_active_session_continua(uuid, text) from public;
revoke execute on function public.registrar_coleta_continua(uuid, text, uuid, collection_type, integer, integer, numeric, numeric) from public;
revoke execute on function public.encerrar_sessao_maquina(uuid, text, uuid) from public;

grant execute on function public.get_active_session_continua(uuid, text) to anon;
grant execute on function public.registrar_coleta_continua(uuid, text, uuid, collection_type, integer, integer, numeric, numeric) to anon;
grant execute on function public.encerrar_sessao_maquina(uuid, text, uuid) to anon;
