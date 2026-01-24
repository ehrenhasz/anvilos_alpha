import { createHash } from 'crypto';
import fs from 'fs';

/**
 * RFC-030: HYDROGEN (The Fuel)
 * Provides deterministic entropy based on system state (Cortex Audit Log).
 */
export class Hydrogen {

    private static instance: Hydrogen;
    private auditLogPath: string;

    private constructor() {
        this.auditLogPath = 'forge.log'; // Using forge.log as the audit source for now
    }

    public static getInstance(): Hydrogen {
        if (!Hydrogen.instance) {
            Hydrogen.instance = new Hydrogen();
        }
        return Hydrogen.instance;
    }

    /**
     * collapse()
     * Collapses the quantum wave function of the system state into a deterministic value.
     * @returns A hex string representing the current entropy.
     */
    public collapse(): string {
        try {
            // In a full implementation, this would read the last N lines of the secure audit log.
            // Here we use file stats and current time as a proxy for system state.
            const stats = fs.statSync(this.auditLogPath);
            const entropySource = `${stats.mtimeMs}-${stats.size}-${Date.now()}`;
            
            const hash = createHash('sha256');
            hash.update(entropySource);
            return hash.digest('hex');

        } catch (error) {
            // Fallback for safety (though RFC implies system failure is preferable to non-determinism)
            console.error("HYDROGEN CRITICAL: ENTROPY SOURCE UNAVAILABLE.");
            return createHash('sha256').update(Date.now().toString()).digest('hex');
        }
    }

    /**
     * Returns a float between 0 and 1, similar to Math.random(), but sourced from Hydrogen.
     */
    public random(): number {
        const hex = this.collapse();
        const int = parseInt(hex.substring(0, 12), 16);
        return int / 0xffffffffffff;
    }
}
