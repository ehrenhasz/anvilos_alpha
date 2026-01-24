import React, { useEffect, useRef } from 'react';
import { Terminal } from 'xterm';
import { FitAddon } from 'xterm-addon-fit';
import 'xterm/css/xterm.css';

export const TerminalComponent: React.FC = () => {
    const terminalRef = useRef<HTMLDivElement>(null);
    const wsRef = useRef<WebSocket | null>(null);

    useEffect(() => {
        if (!terminalRef.current) return;

        const term = new Terminal({
            cursorBlink: true,
            theme: {
                background: '#121212',
                foreground: '#f0f0f0',
                cursor: '#00ff00'
            },
            fontFamily: 'monospace',
            fontSize: 14,
        });
        
        const fitAddon = new FitAddon();
        term.loadAddon(fitAddon);
        term.open(terminalRef.current);
        fitAddon.fit();

        const protocol = window.location.protocol === 'https:' ? 'wss' : 'ws';
        const wsUrl = `${protocol}://${window.location.host}/ws/shell`;
        const ws = new WebSocket(wsUrl);
        wsRef.current = ws;

        ws.onopen = () => {
            term.write('\r\n\x1b[1;32mCONNECTED TO ANVIL SHELL (JSON Mode)\x1b[0m\r\n');
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

        term.onData((data) => {
            if (ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'input', data: data }));
            }
        });

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