// Importa a função oficial para criar o cliente JavaScript do Supabase.
import { createClient } from "https://esm.sh/@supabase/supabase-js@2";
// Importa as configurações centralizadas do aplicativo.
import { CONFIG } from "./config.js";

// Cria e exporta uma única instância do cliente Supabase para ser reutilizada pelo aplicativo.
export const supabase = createClient(
    // URL do projeto Supabase.
    CONFIG.SUPABASE_URL,
    // Chave pública do projeto.
    CONFIG.SUPABASE_ANON_KEY,
    {
        auth: {
            // Renova automaticamente o token da sessão quando necessário.
            autoRefreshToken: true,
            // Mantém a sessão persistida para que o usuário não precise entrar novamente a cada acesso.
            persistSession: true,
            // Permite que o Supabase detecte informações de autenticação presentes na URL.
            detectSessionInUrl: true
        }
    }
);