
import React, { useState, useEffect } from 'react';
import { BicameralConfig } from '../types';

interface DeploymentTerminalProps {
  config: BicameralConfig;
  onLog: (msg: string, type: any) => void;
}

export const DeploymentTerminal: React.FC<DeploymentTerminalProps> = ({ config, onLog }) => {
  const [step, setStep] = useState(0);
  const [progress, setProgress] = useState(0);
  const [isDeploying, setIsDeploying] = useState(false);

  const steps = [
    "Establishing VSOCK Umbilical Link...",
    "Connecting to Target Node via Tier: " + config.apiTier,
    "Initiating VirtualMeat Lobotomy (RFC-0009 Conversion)...",
    "Stripping source metadata signatures...",
    "Generating hermetic Bytecode Body...",
    "Injecting PQC-X25519 Cryptographic Salt...",
    "Configuring Monolith Kernel (CONFIG_MODULES=n)...",
    "Sealing Black Box Runtime (Podman Isolation)...",
    "Severing Umbilical. Dark Run Active."
  ];

  const handleDeploy = () => {
    setIsDeploying(true);
    setStep(0);
    setProgress(0);
    onLog("Initiating sovereign deployment sequence...", "deploy");
  };

  useEffect(() => {
    if (!isDeploying) return;

    const timer = setInterval(() => {
      setProgress(prev => {
        if (prev >= 100) {
          if (step < steps.length - 1) {
            setStep(s => s + 1);
            return 0;
          } else {
            setIsDeploying(false);
            onLog("Deployment successfully ratified on target.", "success");
            clearInterval(timer);
            return 100;
          }
        }
        return prev + 6;
      });
    }, 120);

    return () => clearInterval(timer);
  }, [isDeploying, step]);

  return (
    <div className="h-full flex flex-col p-6 font-mono overflow-hidden bg-black border border-green-900/30">
      <div className="flex items-center gap-4 mb-8">
        <div className="w-16 h-16 border-4 border-double border-green-500 bg-green-950/10 flex items-center justify-center text-3xl shadow-[0_0_15px_rgba(0,255,65,0.2)]">
          🚀
        </div>
        <div>
          <h2 className="text-xl font-black orbitron text-green-500 uppercase tracking-widest italic">Black_Ops_Deployer</h2>
          <div className="text-[10px] text-green-900 font-black">VIRTUALMEAT_RUNTIME // SOVEREIGN_CLEARANCE</div>
        </div>
      </div>

      <div className="flex-1 flex flex-col gap-6">
        <div className="bg-black/80 border border-green-900 p-6 flex-1 shadow-[inset_0_0_40px_rgba(0,0,0,1)] relative overflow-hidden">
          {!isDeploying && progress === 0 ? (
            <div className="h-full flex flex-col items-center justify-center gap-4 text-center">
              <span className="text-red-600 font-black animate-pulse orbitron tracking-[0.3em]">SYSTEM_STANDBY</span>
              <p className="text-[10px] text-green-900 max-w-xs uppercase font-black">
                Ready to transmit sovereign bytecode to target. <br/>
                All logic traces will be severed post-deployment.
              </p>
              <button 
                onClick={handleDeploy}
                className="mt-4 bg-green-600 text-black px-10 py-3 font-black orbitron border-4 border-green-400 hover:bg-green-500 hover:shadow-[0_0_30px_rgba(0,255,65,0.4)] active:scale-95 transition-all uppercase"
              >
                Execute_Deployment
              </button>
            </div>
          ) : (
            <div className="space-y-4">
              <div className="flex justify-between items-center text-[10px] font-black text-green-500 uppercase tracking-widest">
                <span>Phase_{step + 1}/{steps.length}: {isDeploying ? 'Running' : 'Complete'}</span>
                <span className="tabular-nums">{progress}%</span>
              </div>
              <div className="w-full h-3 border border-green-900 bg-black overflow-hidden p-0.5">
                <div 
                  className="h-full bg-green-500 transition-all duration-150 ease-linear shadow-[0_0_10px_rgba(0,255,65,0.8)]"
                  style={{ width: `${progress}%` }}
                ></div>
              </div>
              <div className="text-[11px] font-bold text-green-500 mt-6 space-y-2 custom-scrollbar overflow-y-auto max-h-[150px]">
                {steps.slice(0, step + 1).map((s, i) => (
                  <div key={i} className="flex gap-2 items-start animate-in fade-in slide-in-from-left-2 duration-300">
                    <span className={i === step ? "animate-pulse text-green-400 font-black" : "text-green-950"}>{i === step ? ">" : "√"}</span>
                    <span className={i === step ? "text-green-400" : "text-green-950 italic"}>{s}</span>
                  </div>
                ))}
              </div>
              {progress === 100 && step === steps.length - 1 && (
                <div className="mt-8 p-4 border-4 border-double border-green-500 bg-green-950/30 text-center animate-in zoom-in-95 duration-500">
                  <div className="text-2xl font-black orbitron text-green-500 mb-2 uppercase tracking-tighter">Payload_Ratified</div>
                  <div className="text-[10px] text-green-400 uppercase font-black italic">Dark Run Active. Traces Severed. System is sovereign.</div>
                  <button onClick={() => setProgress(0)} className="mt-4 text-[8px] underline text-green-900 uppercase font-black tracking-widest">Reset_Terminal</button>
                </div>
              )}
            </div>
          )}
        </div>

        <div className="bg-green-950/10 border border-green-900/50 p-3 text-[9px] font-black grid grid-cols-2 gap-4 uppercase tracking-tighter shadow-[inset_0_0_10px_rgba(0,0,0,1)]">
          <div className="flex justify-between">
            <span className="text-green-900">Cipher:</span> <span>ML-KEM-768</span>
          </div>
          <div className="flex justify-between">
            <span className="text-green-900">Protocol:</span> <span>VSOCK_PQC</span>
          </div>
          <div className="flex justify-between">
            <span className="text-green-900">Clearance:</span> <span>SOVEREIGN</span>
          </div>
          <div className="flex justify-between">
            <span className="text-green-900">Verdict:</span> <span className={isDeploying ? 'animate-pulse text-green-500' : 'text-green-700'}>{isDeploying ? 'EXECUTING' : 'NOMINAL'}</span>
          </div>
        </div>
      </div>
    </div>
  );
};
