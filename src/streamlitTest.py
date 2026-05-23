import streamlit as st
import firebase_admin
from firebase_admin import credentials, db
import time
import pandas as pd
from datetime import datetime
import plotly.graph_objects as go
import winsound
import os

# 1. Configuração da página
st.set_page_config(
    page_title="Estação de segurança",
    page_icon="🚨",
    layout="wide"
)

# 2. Inicialização segura do Firebase
if not firebase_admin._apps:
    cred = credentials.Certificate("src/mpes-g2-firebase-adminsdk-fbsvc-0ce8312cad.json")
    firebase_admin.initialize_app(cred, {
        'databaseURL': 'https://mpes-g2-default-rtdb.firebaseio.com/'
    })

# 3. Inicialização da Memória da Série Temporal
if 'temperature_history' not in st.session_state:
    st.session_state.temperature_history = []

# Estrutura Visual Fixa
st.title("🛡️ Dashboard de segurança")
st.markdown("Painel de monitoramento com foco em análise térmica, controle de umidade e capacidade.")
st.write("---")

status_placeholder = st.empty()

# Containers de Alertas no topo (Todos com fontes grandes e excelente destaque)
connection_container = st.empty() 
alert_container = st.empty()
capacity_container = st.empty()  
humidity_container = st.empty()  # 🌟 Novo container dedicado para alertas de umidade fora da faixa

# ==========================================
# LINHA 1: ANÁLISE TÉRMICA (Série Temporal e Gauge)
# ==========================================
col_chart, col_gauge = st.columns([2, 1]) 

with col_chart:
    st.subheader("📈 Temperatura/tempo")
    chart_placeholder = st.empty()

with col_gauge:
    st.subheader("🌡️ Gauge de temperatura")
    gauge_placeholder = st.empty()

st.write("---")

# ==========================================
# LINHA 2: OUTRAS INFORMAÇÕES E TELEMETRIA
# ==========================================
st.markdown("### 🔍 Outras informações")
col_temp, col_hum, col_people, col_peripherals = st.columns(4)

with col_temp:
    temp_card = st.empty()

with col_hum:
    hum_card = st.empty()

with col_people:
    people_card = st.empty()

with col_peripherals:
    peripherals_card = st.empty()

st.write("---")


# 4. Fragmento de Atualização Silenciosa a cada 2 segundos
@st.fragment(run_every=2)
def atualizar_dados_do_firebase():
    try:
        ref = db.reference('/')
        data = ref.get()
        
        if data:
            # --- EXTRAÇÃO DA ÁRVORE DO FIREBASE ---
            dict_alarme = data.get('alarme', {})
            dict_ambiente = data.get('ambiente', {})
            dict_config = data.get('config', {})
            dict_pessoas = data.get('pessoas', {})
            dict_sistema = data.get('sistema', {})
            
            # Dados do Alarme
            alarme_ativo = dict_alarme.get('ativo', False)
            alarme_estado = dict_alarme.get('estado', 'normal')
            alarme_temp = dict_alarme.get('temp_alarme', 0.0)
            
            # Dados do Ambiente
            temperature = dict_ambiente.get('temperatura', 0.0)
            humidity = dict_ambiente.get('umidade', 0.0)
            
            # 🌟 NOVOS LIMITES DINÂMICOS DO DICT CONFIG 🌟
            pessoas_max = dict_config.get('pessoas_max', 5)
            umid_max = dict_config.get('umid_max', 55)
            umid_min = dict_config.get('umid_min', 40)
            
            # Limiares de Temperatura existentes para o Gauge
            temp_cold = dict_config.get('temp_cold', 18)
            temp_warn = dict_config.get('temp_warn', 27)
            temp_crit = dict_config.get('temp_crit', 35)
            
            
            # Dados de Fluxo de Pessoas
            pessoas_entradas = dict_pessoas.get('entradas', 0)
            pessoas_saidas = dict_pessoas.get('saidas', 0)
            pessoas_total = dict_pessoas.get('total', 0)
            pessoas_total = dict_pessoas.get('total', 0)
            
            # Estados Periféricos
            button_state = data.get('button', False)
            led_state = data.get('led', False)
            
            modified_date = dict_sistema.get('ultimo_envio', '')
            agora = datetime.now()
            horario_atual = agora.strftime('%H:%M:%S')
            
            # --- SYSTEM WATCHDOG (TIMEOUT CONTROLLER) ---
            hardware_offline = False
            motivo_erro = ""
            
            if not modified_date or modified_date == "No date provided":
                hardware_offline = True
                motivo_erro = "Dados de data/hora ausentes no sistema."
            else:
                try:
                    tempo_hardware = datetime.strptime(modified_date, "%d/%m/%Y %H:%M:%S")
                    diferenca_segundos = (agora - tempo_hardware).total_seconds()
                    
                    if diferenca_segundos > 20:
                        hardware_offline = True
                        motivo_erro = f"O hardware está sem comunicação há {int(diferenca_segundos)} segundos."
                except Exception:
                    hardware_offline = True
                    motivo_erro = "Formato de data inválido recebido do sistema."

            if hardware_offline:
                connection_container.error(f"## 📡 ERRO DE COMUNICAÇÃO: {motivo_erro} Verifique a conexão do hardware.")
                winsound.Beep(1000, 400)
            else:
                connection_container.empty()

            # Atualiza o histórico de temperatura
            st.session_state.temperature_history.append({
                'Time': horario_atual,
                'Temperature (°C)': temperature
            })
            if len(st.session_state.temperature_history) > 30:
                st.session_state.temperature_history.pop(0)
                
            # --- PROCESSAMENTO DOS ALERTAS (FONTES GRANDES) ---
            
            # 1. Alarme Térmico Superior
            if temperature > temp_warn:
                alert_container.error(f"## 🚨 ALERTA CRÍTICO: Temperatura elevada na sala de servidores! ({alarme_estado.upper()}) | Temp Alarme: {alarme_temp:.2f} °C")
                for _ in range(5):  
                    winsound.Beep(2500, 500)  
                    time.sleep(0.1)
            else:
                alert_container.success("### ✅ Status do Sistema: Seguro e em monitoramento")
                
            # 2. Monitoramento de Capacidade Máxima Dinâmica
            if pessoas_total > pessoas_max:
                capacity_container.warning(f"## 🛑 LIMITE EXCEDIDO: {pessoas_total} pessoas na sala! O máximo permitido por configuração é {pessoas_max}.")
            else:
                capacity_container.empty() 
                
            # 3. 🌟 NOVO: Monitoramento de Limiares de Umidade
            if humidity > umid_max:
                humidity_container.error(f"## 💧 ALERTA DE UMIDADE ELEVADA: {humidity:.2f}% UR! Ultrapassou o limite máximo de {umid_max}%.")
            elif humidity < umid_min:
                humidity_container.warning(f"## ⚠️ ALERTA DE UMIDADE BAIXA: {humidity:.2f}% UR! Caiu abaixo do limite mínimo de {umid_min}%.")
            else:
                humidity_container.empty()
                
            # Rodapé de Sincronização Ampliado
            status_placeholder.markdown(f"""
                ### ⏱️ Informações de Sincronização
                * **Sincronização do Navegador:** {horario_atual}
                * **Última modificação do sistema (Firebase):** {modified_date if modified_date else 'Nenhum registro encontrado'}
            """)

            # --- ATUALIZAÇÃO DOS GRÁFICOS (LINHA 1) ---
            
            # Série Temporal
            df_historico = pd.DataFrame(st.session_state.temperature_history)
            chart_placeholder.line_chart(df_historico.set_index('Time'))
            
            # Gráfico Gauge
            fig_gauge = go.Figure(go.Indicator(
                mode = "gauge+number",
                value = temperature,
                domain = {'x': [0, 1], 'y': [0, 1]},
                number = {'suffix': " °C", 'font': {'size': 36}},
                gauge = {
                    'axis': {'range': [0, 50], 'tickwidth': 1, 'tickcolor': "gray"}, 
                    'bar': {'color': "rgba(0,0,0,0)"}, 
                    'bgcolor': "white",
                    'borderwidth': 2,
                    'bordercolor': "gray",
                    'steps': [
                        {'range': [0, temp_cold], 'color': '#00ff00'},          
                        {'range': [temp_cold, temp_warn], 'color': '#fffb00'},  
                        {'range': [temp_warn, temp_crit], 'color': '#ff9900'},  
                        {'range': [temp_crit, 50], 'color': '#ff0000'}          
                    ],
                    'threshold': {
                        'line': {'color': "#2c3e50", 'width': 5},
                        'thickness': 1.0,
                        'value': temperature 
                    }
                }
            ))
            
            fig_gauge.update_layout(
                margin=dict(l=20, r=20, t=30, b=20),
                height=250,
                paper_bgcolor="rgba(0,0,0,0)",
            )
            gauge_placeholder.plotly_chart(fig_gauge, use_container_width=True)
            
            # --- ATUALIZAÇÃO DOS CARDS DE TEXTO (LINHA 2) ---
            
            temp_card.metric(label="🌡️ Temperatura Atual", value=f"{temperature:.2f} °C")
            hum_card.metric(label="💧 Umidade Relativa", value=f"{humidity:.2f} %")
            
            people_card.metric(
                label="👥 Pessoas na Sala", 
                value=f"{pessoas_total} total",
                delta=f"+{pessoas_entradas} | -{pessoas_saidas} fluxo"
            )
            
            peripherals_card.markdown(f"""
            **🔌 Estado dos Periféricos:**
            * 🔘 Botão: `{"Pressionado" if button_state else "Solto"}`
            * 💡 Status LED: `{"LIGADO" if led_state else "DESLIGADO"}`
            """)

    except Exception as error:
        status_placeholder.error(f"Sync error: {error}")

# Executa o fragmento assíncrono
atualizar_dados_do_firebase()