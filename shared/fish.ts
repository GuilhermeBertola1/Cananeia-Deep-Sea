import { Tile } from './map.ts';

export interface Species {
  id: string;
  name: string;
  price: number; // pago pelo lojista
  weight: number; // chance relativa dentro do bioma
}

export const SPECIES: Record<string, Species> = {
  sardinha: { id: 'sardinha', name: 'Sardinha', price: 1, weight: 60 },
  camarao: { id: 'camarao', name: 'Camarão', price: 2, weight: 35 },
  caranguejo: { id: 'caranguejo', name: 'Caranguejo', price: 4, weight: 5 },
  robalo: { id: 'robalo', name: 'Robalo', price: 4, weight: 50 },
  atum: { id: 'atum', name: 'Atum', price: 6, weight: 40 },
  tartaruga: { id: 'tartaruga', name: 'Tartaruga', price: 12, weight: 10 },
  polvo: { id: 'polvo', name: 'Polvo', price: 10, weight: 50 },
  espada: { id: 'espada', name: 'Peixe-espada', price: 15, weight: 40 },
  lula: { id: 'lula', name: 'Lula-gigante', price: 40, weight: 10 },
};

// Cada bioma tem sua fauna: mar fundo paga mais.
export const BIOME_FISH: Partial<Record<Tile, string[]>> = {
  [Tile.Shallow]: ['sardinha', 'camarao', 'caranguejo'],
  [Tile.Water]: ['robalo', 'atum', 'tartaruga'],
  [Tile.Deep]: ['polvo', 'espada', 'lula'],
};

export function rollSpecies(tile: Tile, r: number): string | null {
  const pool = BIOME_FISH[tile];
  if (!pool) return null;
  const total = pool.reduce((s, id) => s + SPECIES[id].weight, 0);
  let pick = r * total;
  for (const id of pool) {
    pick -= SPECIES[id].weight;
    if (pick <= 0) return id;
  }
  return pool[pool.length - 1];
}

export const holdValue = (hold: Record<string, number>) =>
  Object.entries(hold).reduce((s, [id, n]) => s + SPECIES[id].price * n, 0);
