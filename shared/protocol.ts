// Mensagens trocadas pelo WebSocket. O cliente só envia intenções;
// o servidor decide tudo e devolve o estado visível.

export const TICK_MS = 100;
export const VIEW_RADIUS = 50;

export type ClientMsg =
  | { t: 'join'; name: string }
  | { t: 'input'; dx: number; dy: number }
  | { t: 'sell' };

export interface PlayerView {
  id: string;
  name: string;
  x: number;
  y: number;
  color: number;
}

export interface SchoolView {
  id: number;
  x: number;
  y: number;
  r: number;
  species: string;
}

export interface SelfState extends PlayerView {
  money: number;
  hold: Record<string, number>;
  holdCount: number;
  capacity: number;
  speed: number;
  atDock: boolean;
}

// Pontos de interesse para o minimapa (o mapa inteiro, não só a área visível).
// Bosses, NPCs e missões entram aqui nas próximas fases.
export interface Objective {
  kind: 'school';
  x: number;
  y: number;
  species: string;
}

export interface RankEntry {
  name: string;
  money: number;
}

export type ServerMsg =
  | { t: 'welcome'; id: string; seed: number }
  | {
      t: 'state';
      tick: number;
      you: SelfState;
      players: PlayerView[];
      schools: SchoolView[];
      rank: RankEntry[];
      online: number;
      log: string[];
      objectives?: Objective[]; // enviado a cada segundo
    };
