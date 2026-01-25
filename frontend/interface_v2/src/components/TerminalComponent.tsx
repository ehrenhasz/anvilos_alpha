import React, { useEffect, useRef } from 'react';
import { Terminal } from 'xterm';
import { FitAddon } from 'xterm-addon-fit';
import 'xterm/css/xterm.css';

export const TerminalComponent: React.FC = () => {
    const terminalRef = useRef<HTMLDivElement>(null);
    const wsRef = useRef<WebSocket | null>(null);

    useEffect(() => {
        if (!terminalRef.current) return;

        // Initialize xterm.js
        const term = new Terminal({
            cursorBlink: true,
            theme: {
                background: '#000000',
                foreground: '#e0e0e0',
                cursor: '#00ff00',
                selectionBackground: '#00ff0044'
            },
            fontFamily: 'Menlo, Monaco, "Courier New", monospace',
            fontSize: 14,
        });
        
        const fitAddon = new FitAddon();
        term.loadAddon(fitAddon);
        term.open(terminalRef.current);
        fitAddon.fit();

        // Connect to Interceptor Bridge (RFC-0051)
        const protocol = window.location.protocol === 'https:' ? 'wss' : 'ws';
        // Vite proxy handles /ws -> port 8001
        const wsUrl = `${protocol}://${window.location.host}/ws/shell`;
        const ws = new WebSocket(wsUrl);
        wsRef.current = ws;

        ws.onopen = () => {
            term.write('\r\n\x1b[1;36m[SYSTEM] INTERFACE V2 ONLINE.\x1b[0m\r\n');
            term.write('\x1b[1;30m[BRIDGE] CONNECTED TO 127.0.0.1:8001\x1b[0m\r\n\r\n');
            fitAddon.fit();
            ws.send(JSON.stringify({
                type: 'resize', 
                cols: term.cols, 
                rows: term.rows
            }));
        };

        ws.onmessage = (event) => {
            term.write(event.data);
        };

        ws.onclose = () => {
            term.write('\r\n\x1b[1;31m[SYSTEM] CONNECTION LOST.\x1b[0m\r\n');
        };

        term.onData((data) => {
            if (ws.readyState === WebSocket.OPEN) {
                // Send as strictly typed JSON (RFC-0050)
                ws.send(JSON.stringify({ type: 'input', data: data }));
            }
        });

        // Handle Resize
        const resizeObserver = new ResizeObserver(() => {
            if (!terminalRef.current) return;
            try {
                fitAddon.fit();
                if (ws.readyState === WebSocket.OPEN) {
                    ws.send(JSON.stringify({ 
                        type: 'resize', 
                        cols: term.cols, 
                        rows: term.rows 
                    }));
                }
            } catch (e) {}
        });
        
        resizeObserver.observe(terminalRef.current);

        return () => {
            resizeObserver.disconnect();
            ws.close();
            term.dispose();
        };
    }, []);

    return <div ref={terminalRef} style={{ width: '100%', height: '100%', overflow: 'hidden' }} />;
};
