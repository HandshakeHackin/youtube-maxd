"""Route optional artwork away from provider/control HTTP requests."""


def apply(replace, player, work):
    path = player + "addons/src/evo_net.c"
    replace(path,
        """static pthread_t       g_worker_thread;
static pthread_mutex_t g_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_queue_cond  = PTHREAD_COND_INITIALIZER;
static int             g_running     = 0;

static evo_net_req_t  *g_pending_queue[EVO_NET_MAX_QUEUE];
static int             g_pending_count = 0;
""",
        """enum { EVO_NET_CONTROL = 0, EVO_NET_IMAGE = 1, EVO_NET_LANES = 2 };
static pthread_t       g_worker_thread[EVO_NET_LANES];
static pthread_mutex_t g_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_queue_cond[EVO_NET_LANES] = {
    PTHREAD_COND_INITIALIZER, PTHREAD_COND_INITIALIZER
};
static int             g_running = 0;

static evo_net_req_t  *g_pending_queue[EVO_NET_LANES][EVO_NET_MAX_QUEUE];
static int             g_pending_count[EVO_NET_LANES] = {0, 0};
/* Accepted work includes pending, running, and completed requests. */
static int             g_outstanding_count[EVO_NET_LANES] = {0, 0};

/* Defined below with the existing image failure classification. */
static int url_is_image(const char *url);
""")

    replace(path,
        """    (void)arg;

    while (g_running) {
        evo_net_req_t *req = NULL;

        pthread_mutex_lock(&g_queue_mutex);
        while (g_running && g_pending_count == 0) {
            pthread_cond_wait(&g_queue_cond, &g_queue_mutex);
        }

        if (!g_running) {
            pthread_mutex_unlock(&g_queue_mutex);
            break;
        }

        /* Dequeue first pending item */
        req = g_pending_queue[0];
        for (int i = 0; i < g_pending_count - 1; i++) {
            g_pending_queue[i] = g_pending_queue[i + 1];
        }
        g_pending_count--;
        pthread_mutex_unlock(&g_queue_mutex);
""",
        """    const int lane = (int)(intptr_t)arg;

    for (;;) {
        evo_net_req_t *req = NULL;

        pthread_mutex_lock(&g_queue_mutex);
        while (g_running && g_pending_count[lane] == 0) {
            pthread_cond_wait(&g_queue_cond[lane], &g_queue_mutex);
        }

        if (!g_running) {
            pthread_mutex_unlock(&g_queue_mutex);
            break;
        }

        /* Dequeue first pending item in this lane. */
        req = g_pending_queue[lane][0];
        for (int i = 0; i < g_pending_count[lane] - 1; i++) {
            g_pending_queue[lane][i] = g_pending_queue[lane][i + 1];
        }
        g_pending_count[lane]--;
        pthread_mutex_unlock(&g_queue_mutex);
""")

    replace(path,
        """    g_running = 1;
    g_pending_count = 0;
    g_completed_count = 0;

    if (pthread_create(&g_worker_thread, NULL, evo_net_worker, NULL) != 0) {
        g_running = 0;
        return -1;
    }

    return 0;
""",
        """    g_running = 1;
    for (int lane = 0; lane < EVO_NET_LANES; lane++) g_pending_count[lane] = 0;
    for (int lane = 0; lane < EVO_NET_LANES; lane++) g_completed_count[lane] = 0;
    for (int lane = 0; lane < EVO_NET_LANES; lane++) g_outstanding_count[lane] = 0;

    for (int lane = 0; lane < EVO_NET_LANES; lane++) {
        if (pthread_create(&g_worker_thread[lane], NULL, evo_net_worker,
                           (void *)(intptr_t)lane) != 0) {
            pthread_mutex_lock(&g_queue_mutex);
            g_running = 0;
            for (int started = 0; started < lane; started++)
                pthread_cond_broadcast(&g_queue_cond[started]);
            pthread_mutex_unlock(&g_queue_mutex);
            for (int started = 0; started < lane; started++)
                pthread_join(g_worker_thread[started], NULL);
            return -1;
        }
    }

    return 0;
""")

    replace(path,
        """    g_running = 0;
    pthread_mutex_lock(&g_queue_mutex);
    pthread_cond_broadcast(&g_queue_cond);
    pthread_mutex_unlock(&g_queue_mutex);

    pthread_join(g_worker_thread, NULL);

    /* Free pending */
    for (int i = 0; i < g_pending_count; i++) {
        free_req(g_pending_queue[i]);
    }
    g_pending_count = 0;
""",
        """    pthread_mutex_lock(&g_queue_mutex);
    g_running = 0;
    for (int lane = 0; lane < EVO_NET_LANES; lane++)
        pthread_cond_broadcast(&g_queue_cond[lane]);
    pthread_mutex_unlock(&g_queue_mutex);

    for (int lane = 0; lane < EVO_NET_LANES; lane++)
        pthread_join(g_worker_thread[lane], NULL);

    /* Free both pending queues after the workers have retired. */
    for (int lane = 0; lane < EVO_NET_LANES; lane++) {
        for (int i = 0; i < g_pending_count[lane]; i++)
            free_req(g_pending_queue[lane][i]);
        g_pending_count[lane] = 0;
    }
""")

    replace(path,
        """    pthread_mutex_lock(&g_queue_mutex);
    if (g_pending_count >= EVO_NET_MAX_QUEUE) {
        pthread_mutex_unlock(&g_queue_mutex);
        free_req(req);
        return EVO_NET_ASYNC_ERR_FULL;
    }

    g_pending_queue[g_pending_count++] = req;
    pthread_cond_signal(&g_queue_cond);
    pthread_mutex_unlock(&g_queue_mutex);
""",
        """    const int lane = url_is_image(url) ? EVO_NET_IMAGE : EVO_NET_CONTROL;
    pthread_mutex_lock(&g_queue_mutex);
    if (!g_running || g_outstanding_count[lane] >= EVO_NET_MAX_QUEUE) {
        pthread_mutex_unlock(&g_queue_mutex);
        free_req(req);
        return EVO_NET_ASYNC_ERR_FULL;
    }

    g_pending_queue[lane][g_pending_count[lane]++] = req;
    g_outstanding_count[lane]++;
    pthread_cond_signal(&g_queue_cond[lane]);
    pthread_mutex_unlock(&g_queue_mutex);
""")

    replace(path,
        """static evo_net_req_t  *g_completed_queue[EVO_NET_MAX_QUEUE];
static int             g_completed_count = 0;
""",
        """static evo_net_req_t  *g_completed_queue[EVO_NET_LANES][EVO_NET_MAX_QUEUE];
static int             g_completed_count[EVO_NET_LANES] = {0, 0};
""")

    replace(path,
        """            if (g_completed_count < EVO_NET_MAX_QUEUE) {
                g_completed_queue[g_completed_count++] = req;
            } else {
""",
        """            if (g_completed_count[lane] < EVO_NET_MAX_QUEUE) {
                g_completed_queue[lane][g_completed_count[lane]++] = req;
            } else {
""")

    replace(path,
        """    /* Free completed */
    for (int i = 0; i < g_completed_count; i++) {
        free_req(g_completed_queue[i]);
    }
    g_completed_count = 0;
""",
        """    /* Free completed callbacks in both lanes. */
    for (int lane = 0; lane < EVO_NET_LANES; lane++) {
        for (int i = 0; i < g_completed_count[lane]; i++)
            free_req(g_completed_queue[lane][i]);
        g_completed_count[lane] = 0;
    }
""")

    replace(path,
        """    evo_net_req_t *ready[EVO_NET_MAX_QUEUE];
    int count = 0;

    pthread_mutex_lock(&g_queue_mutex);
    count = g_completed_count;
    for (int i = 0; i < count; i++) {
        ready[i] = g_completed_queue[i];
    }
    g_completed_count = 0;
    pthread_mutex_unlock(&g_queue_mutex);
""",
        """    evo_net_req_t *ready[EVO_NET_LANES * EVO_NET_MAX_QUEUE];
    int count = 0;

    pthread_mutex_lock(&g_queue_mutex);
    /* Dispatch control completions first; artwork cannot consume their slots. */
    for (int lane = 0; lane < EVO_NET_LANES; lane++) {
        for (int i = 0; i < g_completed_count[lane]; i++)
            ready[count++] = g_completed_queue[lane][i];
        g_outstanding_count[lane] -= g_completed_count[lane];
        g_completed_count[lane] = 0;
    }
    pthread_mutex_unlock(&g_queue_mutex);
""")

    replace(path,
        """        if ((size_t)(end - url) >= l && strncasecmp(end - l, ext[i], l) == 0) return 1;
    }
    return 0;
}
""",
        """        if ((size_t)(end - url) >= l && strncasecmp(end - l, ext[i], l) == 0) return 1;
    }
    /* Emby/Jellyfin artwork has a suffixless /Images/Primary API path. */
    for (const char *p = url; end - p >= 8; ++p) {
        if (strncasecmp(p, "/Images/", 8) == 0) return 1;
    }
    return 0;
}
""")
