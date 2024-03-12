import z from 'zod';

export const diviceSchema = z.object({
  id: z.string(),
  'is_active': z.boolean(),
  'is_private_session': z.boolean(),
  'is_restricted': z.boolean(),
  'name': z.string(),
  'type': z.string(),
  'volume_percent': z.number().nullable(),
  'supports_volume': z.boolean()
})

export type Divice = z.infer<typeof diviceSchema>;
