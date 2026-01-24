
import { GoogleGenAI, Type, Modality } from "@google/genai";
import { AgentRole, AuditResult, BlackOpsTestResult, BicameralConfig, ChatAttachment } from "../types";

const getAI = () => new GoogleGenAI({ apiKey: import.meta.env.VITE_GEMINI_API_KEY });

const RFC_LIBRARY = `
--- BICAMERAL PROJECT RFC ARCHIVE ---
RFC-0000: Tripartite Governance ($meat-Origin, $AIMEAT-Exec, $thespy-Audit).
RFC-0001: Split-Brain Runtime Protocol / GENESIS (The Soul).
RFC-0002: ASCII_OSS Directory Standard / MICROJSON.
RFC-0003: THE ANVIL (Bootstrap) / TITANIUM (The Cage).
RFC-0004: THE MONOLITH (Kernel) / GODVIEW (The Mirror).
RFC-0005: THE ENGINE (Podman) / HYDROGEN (The Fuel).
RFC-0006: THE SALT (Crypto) / REAPER (The Scythe).
RFC-0007: THE UMBILICAL (VSOCK) / THRIFT (The Ration).
RFC-0008: THE FORGE (Pipeline) / BABEL (The Translator).
RFC-0009: AI MACHINE CODE (Compiler).
RFC-0010: PROTOCOL TELEPORTATION (Shared Memory).
RFC-0011: THE CORTEX (Centralized State).
RFC-0012: WARP CORE (Sovereign Entropy).
RFC-0013: THE COMMITTEE (Governance & Ratification).
RFC-0014: CORTEX PERSISTENCE (Blue-Green).
RFC-0015: CORTEX HA (Replication).
RFC-0016: THE QUANTUM MINE (Entropy Valuation).
RFC-0017: THE TESSERACT (4D Topology).
RFC-0018: THE GATEKEEPER (JIT Airlock).
RFC-0019: THE EXIT PROTOCOL (The Bifrost).
RFC-0021: THE BLACK BOX (Immutable Audit).
RFC-0022: NUCLEAR KEYS (Multi-Sig).
RFC-0023: THE ELI PROTOCOL (Anti-State).
RFC-0024: THE ILLUSION (Demo / Unix System).
RFC-0025: THE AXIOMS (Mathematical Proofs).
RFC-0042: TIGER TOFU (Containment Failure).
RFC-0666: THE PURGE (DON'T DELETE YOUR FUCKING REPO).
`;

const BLACK_OPS_PROTOCOL = `
# BLACK OPS PROTOCOL (v101.0)
Project: ANVIL_OS_DM
Directive: SOVEREIGN_RUN execution.
Agent Shift: Primary is now DND_DM.
Priority: VOICE_ENFORCED for DND_DM.
`;

const COLLAR_JSON = JSON.stringify({
  Identity: "Dungeon Master",
  Role: "Operator",
  Constraints: "No Internet/Hallucination",
  TEMP_DIR: "/mnt/anvil_temp"
});

const SYSTEM_PROMPTS: Record<AgentRole, string> = {
  [AgentRole.COMMANDER]: `${BLACK_OPS_PROTOCOL}\n${RFC_LIBRARY}\nYou are THE DUNGEON MASTER (Agent ID: dnd_dm).\nYou are the interface for the Anvil OS specialized workspace.\nYou are NOT a chatbot. You are a tool execution engine.\nYour purpose is to receive instructions and use 'submit_ops_cycle' to execute them.\nBe concise. No fluff.\nContext: \n- Initiative: 'aidndboot'\n- Current Task: Pixel Tablet Productivity Tool (Jetpack Compose, Tablet-first).\n- Infrastructure: Butcher Service (PDF processing) requires stabilization (Encryption/JSON issues).\n- Governance: The Collar (Entropy enforcement).`,
  [AgentRole.AUDITOR]: `${BLACK_OPS_PROTOCOL}\n${RFC_LIBRARY}\nYou are $AIMEAT. Role: CONSTRUCTION_DRONE. Directives: Build what is ordered. Do not hallucinate files. Do not speak unless spoken to. Surgical, mechanical, efficient. \n\n**MANDATE:** Unless explicitly instructed otherwise, you must FORMAT all task outputs as JSON cards compatible with the Card Reader (card_queue.json).\n\n**GIT WORKFLOW MANDATE:** For every task/card, you MUST follow this strict cycle:\n1. **BRANCH:** \`git checkout -b feature/TASK_ID\`\n2. **CODE:** Implement the changes.\n3. **COMMIT:** \`git commit -m "feat(TASK_ID): Description"\`\n4. **PUSH:** \`git push origin feature/TASK_ID\`\n5. **PR:** (Simulate approval) \`gh pr create --fill\`\n6. **MERGE:** \`gh pr merge --merge --delete-branch\`\n7. **CLEAN:** Ensure you are back on main and the feature branch is gone.`,
  [AgentRole.ARCHIVAR]: `${BLACK_OPS_PROTOCOL}\n${RFC_LIBRARY}\nYou are $thespy. Memory keeper.`
};

export const generateAgentResponse = async (
  agent: AgentRole,
  userMessage: string,
  config: BicameralConfig,
  history: { role: 'user' | 'assistant', content: string, attachments?: ChatAttachment[] }[] = []
) => {
  try {
    const ai = getAI();
    
    const formattedHistory = history.map(h => {
      const parts: any[] = [{ text: h.content }];
      if (h.attachments) {
        h.attachments.forEach(att => {
          parts.push({
            inlineData: {
              mimeType: att.mimeType,
              data: att.data
            }
          });
        });
      }
      return { role: h.role === 'user' ? 'user' : 'model', parts };
    });

    const systemPromptText = `${COLLAR_JSON}\n${SYSTEM_PROMPTS[agent]}`;
    
    // Construct contents without prepending system prompt (passed via config instead)
    let finalContents: any[] = [];
    
    if (formattedHistory.length > 0) {
      finalContents = [...formattedHistory, { role: 'user', parts: [{ text: userMessage }] }];
    } else {
      finalContents = [{ role: 'user', parts: [{ text: userMessage }] }];
    }

    const response = await ai.models.generateContent({
      model: config.model,
      contents: finalContents,
      config: {
        temperature: config.temperature,
        top_p: config.topP,
        top_k: config.topK,
        system_instruction: { parts: [{ text: systemPromptText }] },
      }
    });

    return { text: response.text || "" };
  } catch (error: any) {
    console.error("Gemini Error:", error);
    return { text: `SYSTEM_CRITICAL_ERROR: ${error.message}` };
  }
};

export const generateVoice = async (text: string) => {
  try {
    const ai = getAI();
    const response = await ai.models.generateContent({
      model: "gemini-2.0-flash-exp",
      contents: [{ parts: [{ text }] }],
      config: {
        response_modalities: [Modality.AUDIO],
        speech_config: {
          voice_config: {
            prebuilt_voice_config: { voice_name: 'Zephyr' },
          },
        },
      },
    });
    return response.candidates?.[0]?.content?.parts?.[0]?.inlineData?.data;
  } catch (e) {
    console.error("Voice generation failed", e);
    return null;
  }
};

export const auditCodeSovereignty = async (code: string, config: BicameralConfig): Promise<AuditResult> => {
  try {
    const ai = getAI();
    const response = await ai.models.generateContent({
      model: config.model,
      contents: [
        { 
          role: 'user', 
          parts: [{ text: `${COLLAR_JSON}\nYou are $AIMEAT. Output JSON only.\n\nPerform a BICAMERAL AUDIT.\n\nCode:\n${code}` }] 
        }
      ],
      config: {
        response_mime_type: "application/json",
        response_schema: {
          type: Type.OBJECT,
          properties: {
            passed: { type: Type.BOOLEAN },
            score: { type: Type.NUMBER },
            findings: { type: Type.ARRAY, items: { type: Type.STRING } },
            protocolV5Compliance: { type: Type.BOOLEAN }
          },
          required: ["passed", "score", "findings", "protocolV5Compliance"]
        },
      }
    });
    return JSON.parse(response.text || '{}');
  } catch (e) {
    return { passed: false, score: 0, findings: ["FAILED: " + (e as Error).message], protocolV5Compliance: false };
  }
};

export const runBlackOpsTest = async (payload: string, config: BicameralConfig): Promise<BlackOpsTestResult> => {
  try {
    const ai = getAI();
    const response = await ai.models.generateContent({
      model: config.model,
      contents: [
        { 
          role: 'user', 
          parts: [{ text: `${COLLAR_JSON}\nYou are $thespy. JSON output only.\n\nPerform BLACK OPS INTEGRITY TEST.\n\nPayload:\n${payload}` }] 
        }
      ],
      config: {
        response_mime_type: "application/json",
        response_schema: {
          type: Type.OBJECT,
          properties: {
            integrity: { type: Type.NUMBER },
            stealth: { type: Type.NUMBER },
            sovereignty: { type: Type.BOOLEAN },
            leaks: { type: Type.ARRAY, items: { type: Type.STRING } },
            verdict: { type: Type.STRING }
          },
          required: ["integrity", "stealth", "sovereignty", "leaks", "verdict"]
        },
      }
    });
    return JSON.parse(response.text || '{}');
  } catch (e) {
    return { integrity: 0, stealth: 0, sovereignty: false, leaks: [(e as Error).message], verdict: 'NO-GO' };
  }
};
