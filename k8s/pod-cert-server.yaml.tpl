apiVersion: v1
kind: ServiceAccount
metadata:
  name: pod-cert-server

---
kind: Role
apiVersion: rbac.authorization.k8s.io/v1
metadata:
  name: pod-cert-server
rules:
- apiGroups: [""]
  resources: ["secrets"]
  verbs: ["get", "create"]
- apiGroups: ["extensions"]
  resources: ["replicasets", "deployments"]
  verbs: ["get", "list"]
- apiGroups: [""]
  resources: ["pods", "services"]
  verbs: ["get", "list"]

---
kind: Role
apiVersion: rbac.authorization.k8s.io/v1
metadata:
  name: pod-cert-server
  namespace: kube-system
rules:
- apiGroups: [""]
  resources: ["secrets"]
  verbs: ["get", "create"]
---

kind: RoleBinding
apiVersion: rbac.authorization.k8s.io/v1
metadata:
  name: pod-cert-server--current
subjects:
- kind: ServiceAccount
  name: pod-cert-server
roleRef:
   kind: Role
   name: pod-cert-server
   apiGroup: rbac.authorization.k8s.io

---
kind: RoleBinding
apiVersion: rbac.authorization.k8s.io/v1
metadata:
  name: pod-cert-server--{{namespace}}
  namespace: kube-system
subjects:
- kind: ServiceAccount
  name: pod-cert-server
  namespace: {{namespace}}
roleRef:
  kind: Role
  name: pod-cert-server
  apiGroup: rbac.authorization.k8s.io

---
apiVersion: apps/v1
kind: Deployment
metadata:
  name: pod-cert-server
  labels:
    app: pod-cert-server
spec:
  replicas: 1
  selector:
    matchLabels:
      app: pod-cert-server
  template:
    metadata:
      labels:
        app: pod-cert-server
    spec:
      serviceAccountName: pod-cert-server
      containers:
      - name: k8s-pod-cert-server
        image: cubeearth/k8s-cert-server:latest
        ports:
        - containerPort: 80
        - containerPort: 443
        env:
        - name: POD_NAMESPACE
          valueFrom:
            fieldRef:
              fieldPath: metadata.namespace
        
---   
apiVersion: v1
kind: Service
metadata:
  name: pod-cert-server
spec:
  selector:
    app: pod-cert-server
  ports:
  - name: http
    port: 80
  - name: https
    port: 443
