import { z } from 'zod'

export const playbackStateSchema = z.object({
  device: z.object({
    id: z.string(),
    is_active: z.boolean(),
    is_private_session: z.boolean(),
    is_restricted: z.boolean(),
    name: z.string(),
    type: z.string(),
    volume_percent: z.number(),
    supports_volume: z.boolean()
  }).optional(),
  repeat_state: z.string().optional(),
  shuffle_state: z.boolean().optional(),
  context: z.object({
    type: z.string(),
    href: z.string(),
    external_urls: z.object({ spotify: z.string() }),
    uri: z.string()
  }).optional(),
  timestamp: z.number(),
  progress_ms: z.number(),
  is_playing: z.boolean(),
  item: z.object({
    album: z.object({
      album_type: z.string(),
      total_tracks: z.number(),
      available_markets: z.array(z.string()),
      external_urls: z.object({ spotify: z.string() }),
      href: z.string(),
      id: z.string(),
      images: z.array(
        z.object({ url: z.string(), height: z.number(), width: z.number() })
      ),
      name: z.string(),
      release_date: z.string(),
      release_date_precision: z.string(),
      type: z.string(),
      uri: z.string(),
      artists: z.array(
        z.object({
          external_urls: z.object({ spotify: z.string() }),
          href: z.string(),
          id: z.string(),
          name: z.string(),
          type: z.string(),
          uri: z.string()
        })
      )
    }),
    artists: z.array(
      z.object({
        external_urls: z.object({ spotify: z.string() }),
        href: z.string(),
        id: z.string(),
        name: z.string(),
        type: z.string(),
        uri: z.string()
      })
    ),
    available_markets: z.array(z.string()),
    disc_number: z.number(),
    duration_ms: z.number(),
    explicit: z.boolean(),
    external_ids: z.object({ isrc: z.string() }),
    external_urls: z.object({ spotify: z.string() }),
    href: z.string(),
    id: z.string(),
    name: z.string(),
    popularity: z.number(),
    preview_url: z.string(),
    track_number: z.number(),
    type: z.string(),
    uri: z.string(),
    is_local: z.boolean()
  }).or(z.null()).optional(),
  currently_playing_type: z.string().optional(),
  actions: z.object({ disallows: z.object({ resuming: z.boolean().optional() }) }).optional(),
  smart_shuffle: z.boolean().optional()
})

export type PlaybackState = z.infer<typeof playbackStateSchema>
