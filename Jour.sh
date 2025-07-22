#!/bin/bash

# Script pour rassembler le contenu de fichiers numérotés
# Usage: ./concat_files.sh [pattern] [output_file]
# Exemple: ./concat_files.sh "fichier*.txt" sortie.txt

# Définir les paramètres par défaut
PATTERN=${1:-"2025_07_21_*"}  # Pattern de recherche (par défaut tous les fichiers)
OUTPUT=${2:-"archive/2025_07_21.csv"}  # Fichier de sortie

# Vider le fichier de sortie s'il existe
> "$OUTPUT"

# Compteur pour suivre le nombre de fichiers traités
count=0

# Fonction pour afficher l'usage
usage() {
    echo "Usage: $0 [pattern] [output_file]"
    echo "Exemple: $0 'fichier*.txt' sortie.txt"
    echo "         $0 'data[0-9]*.log' resultat.txt"
    exit 1
}

# Vérifier si l'aide est demandée
if [[ "$1" == "-h" ]] || [[ "$1" == "--help" ]]; then
    usage
fi

echo "Recherche des fichiers correspondant au pattern: $PATTERN"

# Trier les fichiers par ordre numérique (extraction du numéro)
for file in $(ls $PATTERN 2>/dev/null | sort -V); do
    if [[ -f "$file" ]]; then
        echo "Traitement du fichier: $file"
        
        # Ajouter le contenu du fichier
        cat "$file" >> "$OUTPUT"
        
        # Ajouter un retour à la ligne entre les fichiers
        echo "" >> "$OUTPUT"
        
        ((count++))
    fi
done

# Vérifier si des fichiers ont été trouvés
if [[ $count -eq 0 ]]; then
    echo "Aucun fichier trouvé correspondant au pattern: $PATTERN"
    rm -f "$OUTPUT"  # Supprimer le fichier vide
    exit 1
else
    echo "Concaténation terminée: $count fichier(s) traité(s)"
    echo "Résultat sauvegardé dans: $OUTPUT"
fi

# Efacement des fichiers du jour
 rm $PATTERN 
